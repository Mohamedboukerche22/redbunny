#include "redb.h"

#include <llvm/Config/llvm-config.h>

#include <errno.h>
#include <limits.h>

#ifdef _WIN32
#include <process.h>
#include <io.h>
#include <windows.h>
#define REDB_RUNTIME_FILE "redb_runtime.lib"
#define REDB_ALT_RUNTIME_FILE "libredb_runtime.a"
#define REDB_ACCESS _access
#define REDB_R_OK 4
#else
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#define REDB_RUNTIME_FILE "libredb_runtime.a"
#define REDB_ALT_RUNTIME_FILE "libredb_runtime.a"
#define REDB_ACCESS access
#define REDB_R_OK R_OK
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void usage(FILE *out)
{
    fprintf(out,
        "RedBunny compiler " REDB_VERSION "\n"
        "\n"
        "usage: redb <file.redb> [options]\n"
        "\n"
        "options:\n"
        "  -o <path>       write the executable to <path>\n"
        "  -S, --emit-ir   write LLVM IR only (no link step)\n"
        "      --keep-ir   keep the generated .ll file next to the binary\n"
        "      --run       compile and run the program\n"
        "  -V, --version   print the compiler version\n"
        "  -h, --help      show this help\n"
        "\n"
        "environment:\n"
        "  REDB_CC         C compiler used for linking (default: clang)\n"
        "  REDB_RUNTIME    path to libredb_runtime.a\n");
}

static int file_exists(const char *path)
{
    return path && REDB_ACCESS(path, REDB_R_OK) == 0;
}

static const char *runtime_path(const char *argv0)
{
    static char buf[PATH_MAX + 64];
    static char exe[PATH_MAX];
    const char *env = getenv("REDB_RUNTIME");
    char *slash;
    long n;

    if (file_exists(env))
        return env;

#ifdef REDB_RUNTIME_PATH
    if (file_exists(REDB_RUNTIME_PATH))
        return REDB_RUNTIME_PATH;
#endif

#ifndef _WIN32
    if (file_exists("/usr/local/lib/" REDB_RUNTIME_FILE))
        return "/usr/local/lib/" REDB_RUNTIME_FILE;
    if (file_exists("/usr/lib/" REDB_RUNTIME_FILE))
        return "/usr/lib/" REDB_RUNTIME_FILE;
#endif

#ifdef _WIN32
    n = (long)GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe));
    if (n <= 0)
        snprintf(exe, sizeof(exe), "%s", argv0 ? argv0 : "redb");
#else
    n = (long)readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if (n > 0)
        exe[n] = '\0';
    else
        snprintf(exe, sizeof(exe), "%s", argv0 ? argv0 : "redb");
#endif

    slash = strrchr(exe, '/');
#ifdef _WIN32
    {
        char *backslash = strrchr(exe, '\\');

        if (backslash && (!slash || backslash > slash))
            slash = backslash;
    }
#endif
    if (slash) {
        *slash = '\0';

        snprintf(buf, sizeof(buf), "%s/%s", exe, REDB_RUNTIME_FILE);
        if (file_exists(buf))
            return buf;

        snprintf(buf, sizeof(buf), "%s/%s", exe, REDB_ALT_RUNTIME_FILE);
        if (file_exists(buf))
            return buf;

        snprintf(buf, sizeof(buf), "%s/../lib/%s", exe, REDB_RUNTIME_FILE);
        if (file_exists(buf))
            return buf;

        snprintf(buf, sizeof(buf), "%s/../%s", exe, REDB_RUNTIME_FILE);
        if (file_exists(buf))
            return buf;
    }

    return NULL;
}

static const char *base_name(const char *path, char *out, size_t out_size)
{
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    char *dot;

    snprintf(out, out_size, "%s", name);
    dot = strrchr(out, '.');
    if (dot && !strcmp(dot, ".redb"))
        *dot = '\0';
    if (!out[0])
        snprintf(out, out_size, "a.out");
    return out;
}

static int run_command(char *const argv[])
{
#ifdef _WIN32
    intptr_t rc = _spawnvp(_P_WAIT, argv[0], (const char *const *)argv);

    if (rc < 0) {
        fprintf(stderr, "redb: could not execute '%s': %s\n", argv[0], strerror(errno));
        return -1;
    }
    return (int)rc;
#else
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        fprintf(stderr, "redb: fork failed: %s\n", strerror(errno));
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "redb: could not execute '%s': %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            fprintf(stderr, "redb: waitpid failed: %s\n", strerror(errno));
            return -1;
        }
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return -1;
#endif
}

int main(int argc, char **argv)
{
    const char *input = NULL;
    const char *output = NULL;
    char default_output[PATH_MAX + 16];
    char ir_path[PATH_MAX + 32];
    char *source;
    ASTNode *ast;
    const char *runtime;
    const char *cc;
    int emit_ir = 0;
    int keep_ir = 0;
    int do_run = 0;
    int i;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            usage(stdout);
            return 0;
        }
        if (!strcmp(arg, "-V") || !strcmp(arg, "--version")) {
            printf("redb %s (LLVM %d.%d.%d)\n", REDB_VERSION,
                   LLVM_VERSION_MAJOR, LLVM_VERSION_MINOR, LLVM_VERSION_PATCH);
            return 0;
        }
        if (!strcmp(arg, "-o")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "redb: error: -o requires a path\n");
                return 1;
            }
            output = argv[++i];
            continue;
        }
        if (!strcmp(arg, "-S") || !strcmp(arg, "--emit-ir")) {
            emit_ir = 1;
            continue;
        }
        if (!strcmp(arg, "--keep-ir")) {
            keep_ir = 1;
            continue;
        }
        if (!strcmp(arg, "--run")) {
            do_run = 1;
            continue;
        }
        if (arg[0] == '-') {
            fprintf(stderr, "redb: error: unknown option '%s'\n", arg);
            usage(stderr);
            return 1;
        }
        if (input) {
            fprintf(stderr, "redb: error: more than one input file\n");
            return 1;
        }
        input = arg;
    }

    if (!input) {
        fprintf(stderr, "redb: error: no input file\n");
        usage(stderr);
        return 1;
    }

    source = redb_read_file(input);
    if (!source) {
        fprintf(stderr, "redb: error: cannot read '%s': %s\n", input, strerror(errno));
        return 1;
    }

    if (redb_lex(source, input) != 0) {
        free(source);
        return 1;
    }
    free(source);

    ast = redb_parse(input);
    if (redb_expand_includes(ast, input) != 0) {
        free_ast(ast);
        return 1;
    }
    redb_typecheck(ast);

    if (!output) {
        base_name(input, default_output, sizeof(default_output));
#ifdef _WIN32
        if (!emit_ir)
            strncat(default_output, ".exe",
                    sizeof(default_output) - strlen(default_output) - 1);
#endif
        output = default_output;
    }

    if (emit_ir) {
        if (output == default_output) {
            snprintf(ir_path, sizeof(ir_path), "%s.ll", default_output);
        } else {
            snprintf(ir_path, sizeof(ir_path), "%s", output);
        }
    } else {
        snprintf(ir_path, sizeof(ir_path), "%s.ll", output);
    }

    if (redb_codegen(ast, ir_path) != 0) {
        free_ast(ast);
        return 1;
    }
    free_ast(ast);

    if (emit_ir) {
        printf("redb: wrote %s\n", ir_path);
        return 0;
    }

    runtime = runtime_path(argv[0]);
    if (!runtime) {
        fprintf(stderr,
                "redb: error: cannot find libredb_runtime.a\n"
                "      set REDB_RUNTIME or reinstall the compiler\n");
        return 1;
    }

    cc = getenv("REDB_CC");
    if (!cc || !cc[0])
        cc = "clang";

    {
        char *link_argv[] = {
            (char *)cc,
            "-O2",
            "-Wno-override-module",
            ir_path,
            (char *)runtime,
            "-o",
            (char *)output,
#ifndef _WIN32
            "-lm",
#endif
            NULL
        };
        int rc = run_command(link_argv);

        if (rc != 0) {
            fprintf(stderr, "redb: error: linking failed (IR kept at %s)\n", ir_path);
            return rc > 0 ? rc : 1;
        }
    }

    if (!keep_ir)
        remove(ir_path);

    if (do_run) {
        char run_path[PATH_MAX + 32];
        char *run_argv[] = { run_path, NULL };
        int rc;

#ifdef _WIN32
        snprintf(run_path, sizeof(run_path), "%s", output);
#else
        if (strchr(output, '/'))
            snprintf(run_path, sizeof(run_path), "%s", output);
        else
            snprintf(run_path, sizeof(run_path), "./%s", output);
#endif

        rc = run_command(run_argv);
        if (rc < 0)
            return 1;
        return rc;
    }

    return 0;
}
