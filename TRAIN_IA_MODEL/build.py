#!/usr/bin/env python3
"""Build instruction-tuning data for RedBunny from this repository.

The generator is intentionally dependency-free. It scans the manual, examples,
standard library and test suite, combines them with the curated seed file, and
writes several trainable formats under TRAIN_IA_MODEL/data/.

Usage:
    python3 TRAIN_IA_MODEL/build.py
    python3 TRAIN_IA_MODEL/build.py --check
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
OUT = HERE / "data"
SYSTEM_PROMPT = (HERE / "prompts" / "system.txt").read_text(encoding="utf-8").strip()

DOC_PATH = ROOT / "docs" / "DOCUMENTATION.md"
SOURCE_GLOBS = (
    "README.md",
    "docs/DOCUMENTATION.md",
    "examples/**/*.redb",
    "lib/*.redb",
    "tests/cases/*.redb",
    "tests/cases/*.expected",
    "tests/errors/*.redb",
    "TRAIN_IA_MODEL/build.py",
    "TRAIN_IA_MODEL/seed.jsonl",
    "TRAIN_IA_MODEL/prompts/system.txt",
)

EXAMPLE_PURPOSES = {
    "hello.redb": "printing Hello, World!",
    "tour.redb": "variables, formatting, loops, conditionals, functions and string concatenation",
    "factorial.redb": "recursive factorial with a while loop and a user function",
    "arrays.redb": "array allocation, indexing, resizing, literals and nested arrays",
    "algorithm_demo.redb": "sorting, binary search, reversing and min/max element with the algorithm library",
    "math_demo.redb": "using the standard math library",
    "main.redb": "a user function that adds two integers",
    "imports/app.redb": "local and standard library includes",
    "imports/greeting.redb": "a local module used by another RedBunny file",
}

ERROR_EXPLANATIONS = {
    "array_literal_type_mismatch": "Array literal elements must share one element type. `array<int> a = {1, \"two\"}` mixes `int` and `str`, so the compiler rejects the literal.",
    "array_size_mismatch": "When an explicit array size is written before an initializer, the size must equal the number of literal elements. `array<int> a(2) = {1, 2, 3}` declares size 2 but supplies 3 elements.",
    "array_type_mismatch": "The array element type is fixed by `array<int>`. Assigning `\"text\"` to `a[0]` tries to store a `str` in an `int` slot, which is a compile-time type error.",
    "bad_format": "`writeformat` placeholders must name variables visible in scope. `{missing}` is not a declared variable, so the compiler rejects the format string.",
    "break_outside": "`break` is only valid inside a `for` or `while` loop. This program uses `break` directly in `main()`.",
    "type_mismatch": "A variable keeps the type from its first assignment. `x = 1` makes `x` an `int`, so `x = \"one\"` is rejected.",
    "unknown_var": "Identifiers must be declared before use. `nope` has no visible binding, so `writeln(nope)` is rejected.",
    "wrong_args": "A function call must pass exactly the number of arguments in its declared signature. `f` is declared with one parameter, but this call passes two.",
}


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def sha256_text(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def clean_title(title: str) -> str:
    return re.sub(r"^\d+\.\s*", "", title.strip())


def stable_id(category: str, source: str, instruction: str, output: str) -> str:
    digest = hashlib.sha256(
        "\0".join((category, source, instruction, output)).encode("utf-8")
    ).hexdigest()[:10]
    stem = Path(source).stem or "data"
    return f"{category}:{stem}:{digest}"


REDB_BLOCK = re.compile(r"```redb\n(.*?)```", re.S)


def concise_answer(text: str) -> str:
    """Keep a short intro and the redb code blocks, dropping long prose/tables."""
    blocks = [block.strip() for block in REDB_BLOCK.findall(text) if block.strip()]
    if not blocks:
        text = re.sub(r"```.*?```", "", text, flags=re.S)
        return text.strip()[:500]

    prose = text.split("```", 1)[0]
    lines: list[str] = []
    for line in prose.splitlines():
        line = line.strip()
        if (
            not line
            or line.startswith("#")
            or line.startswith("|")
            or line.startswith("- ")
            or line.startswith("---")
        ):
            continue
        lines.append(line)
    joined = " ".join(lines)
    sentences = [s.strip() for s in re.split(r"(?<=[.!?])\s+", joined) if s.strip()]
    intro = " ".join(sentences[:2]).strip()
    if len(intro) > 320:
        intro = intro[:320].rsplit(" ", 1)[0] + "..."
    parts = [intro] if intro else []
    for block in blocks[:3]:
        parts.append(f"```redb\n{block}\n```")
    return "\n\n".join(parts)


def make_concise(records: list[dict]) -> list[dict]:
    concise: list[dict] = []
    for record in records:
        if record["category"] != "manual":
            concise.append(record)
            continue
        answer = concise_answer(record["output"])
        if not answer:
            continue
        item = dict(record)
        item["id"] = stable_id(
            "manual-concise", record["source"], record["instruction"], answer
        )
        item["category"] = "manual-concise"
        item["output"] = answer
        item["messages"] = [
            record["messages"][0],
            record["messages"][1],
            {"role": "assistant", "content": answer},
        ]
        concise.append(item)
    return concise


class DatasetBuilder:
    def __init__(self) -> None:
        self.records: list[dict] = []
        self._seen: set[tuple[str, str, str]] = set()

    def add(
        self,
        category: str,
        instruction: str,
        output: str,
        *,
        input_text: str = "",
        source: str = "",
    ) -> None:
        instruction = instruction.strip()
        output = output.strip()
        input_text = input_text.strip()
        if not instruction or not output:
            return
        key = (instruction, input_text, output)
        if key in self._seen:
            return
        self._seen.add(key)
        user_content = instruction
        if input_text:
            user_content += "\n\nInput:\n" + input_text
        self.records.append(
            {
                "id": stable_id(category, source, instruction, output),
                "category": category,
                "source": source,
                "messages": [
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": user_content},
                    {"role": "assistant", "content": output},
                ],
                "instruction": instruction,
                "input": input_text,
                "output": output,
            }
        )


def add_seed(builder: DatasetBuilder) -> None:
    path = HERE / "seed.jsonl"
    if not path.exists():
        return
    for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as exc:
            raise SystemExit(f"{rel(path)}:{line_no}: invalid JSON: {exc}") from exc
        builder.add(
            row.get("category", "core"),
            row["instruction"],
            row["output"],
            input_text=row.get("input", ""),
            source=row.get("source", rel(path)),
        )


def iter_doc_sections(text: str):
    """Yield (title, body) for top-level and ### subsections."""
    for block in re.split(r"(?m)^##\s+", text)[1:]:
        if "\n" not in block:
            continue
        top_title, body = block.split("\n", 1)
        top_title = clean_title(top_title)
        subsections = re.split(r"(?m)^###\s+", body)
        intro = subsections[0].strip()
        if intro:
            yield top_title, intro
        for sub in subsections[1:]:
            if "\n" not in sub:
                continue
            sub_title, sub_body = sub.split("\n", 1)
            yield f"{top_title} - {sub_title.strip()}", sub_body.strip()


def add_docs(builder: DatasetBuilder) -> None:
    if not DOC_PATH.exists():
        return
    text = DOC_PATH.read_text(encoding="utf-8")
    wanted = (
        "Hello World",
        "Comments",
        "Program Structure",
        "Variables and Types",
        "Operators",
        "Output",
        "Control Flow",
        "Functions",
        "Includes and Standard Library",
        "Current Limitations",
        "Examples",
    )
    for title, body in iter_doc_sections(text):
        if not any(title.startswith(prefix) for prefix in wanted):
            continue
        body = re.sub(r"\n---\s*$", "", body).strip()
        if not body:
            continue
        builder.add(
            "manual",
            f"Explain RedBunny's {title} with syntax and examples.",
            body,
            source=rel(DOC_PATH),
        )


def add_examples(builder: DatasetBuilder) -> None:
    for path in sorted((ROOT / "examples").rglob("*.redb")):
        code = path.read_text(encoding="utf-8").strip()
        if not code:
            continue
        source = rel(path)
        purpose = EXAMPLE_PURPOSES.get(source) or EXAMPLE_PURPOSES.get(path.name)
        if not purpose:
            purpose = path.stem.replace("_", " ")
        builder.add(
            "example",
            f"Write a complete RedBunny program that demonstrates {purpose}.",
            f"```redb\n{code}\n```",
            source=source,
        )


def add_tests(builder: DatasetBuilder) -> None:
    cases = ROOT / "tests" / "cases"
    for path in sorted(cases.glob("*.redb")):
        code = path.read_text(encoding="utf-8").strip()
        expected_path = path.with_suffix(".expected")
        if not expected_path.exists() or not code:
            continue
        expected = expected_path.read_text(encoding="utf-8").strip()
        source = rel(path)
        builder.add(
            "test-output",
            "Write a RedBunny program that prints exactly this output:\n"
            f"```text\n{expected}\n```",
            f"```redb\n{code}\n```",
            source=source,
        )
        builder.add(
            "code-output",
            f"What does this RedBunny program print?\n```redb\n{code}\n```",
            f"```text\n{expected}\n```",
            source=source,
        )

    errors = ROOT / "tests" / "errors"
    for path in sorted(errors.glob("*.redb")):
        code = path.read_text(encoding="utf-8").strip()
        if not code:
            continue
        explanation = ERROR_EXPLANATIONS.get(
            path.stem,
            "This program is rejected by the RedBunny compiler because it violates the language's static rules.",
        )
        builder.add(
            "compile-error",
            f"Why does this RedBunny program fail to compile?\n```redb\n{code}\n```",
            explanation,
            source=rel(path),
        )


def add_library(builder: DatasetBuilder) -> None:
    definition = re.compile(
        r"^([A-Za-z_]\w*)\s*=\s*(?:do|doto)\s*\((.*?)\)\s*\{\s*$"
    )
    for path in sorted((ROOT / "lib").glob("*.redb")):
        lines = path.read_text(encoding="utf-8").splitlines()
        i = 0
        while i < len(lines):
            match = definition.match(lines[i])
            if not match:
                i += 1
                continue
            name, signature = match.group(1), match.group(2).strip()
            comments: list[str] = []
            j = i - 1
            while j >= 0 and lines[j].strip().startswith("--"):
                comments.append(lines[j].strip())
                j -= 1
            comments.reverse()

            body = [lines[i]]
            i += 1
            while i < len(lines):
                body.append(lines[i])
                if lines[i].rstrip() == "}":
                    i += 1
                    break
                i += 1
            code = "\n".join(body)
            output = ""
            if comments:
                output += "\n".join(comments) + "\n\n"
            output += f"Signature: `({signature})`\n\n```redb\n{code}\n```"
            builder.add(
                "stdlib",
                f"Show the RedBunny standard library function `{name}` and explain its signature.",
                output,
                source=rel(path),
            )


def build_records() -> list[dict]:
    builder = DatasetBuilder()
    add_seed(builder)
    add_docs(builder)
    add_examples(builder)
    add_tests(builder)
    add_library(builder)
    return builder.records


def is_eval(record: dict) -> bool:
    digest = hashlib.sha256(record["id"].encode("utf-8")).hexdigest()
    return int(digest, 16) % 10 == 0


def make_plain_text(records: list[dict]) -> str:
    chunks = ["# RedBunny training corpus", "", "## System prompt", "", SYSTEM_PROMPT, ""]
    for record in records:
        chunks.extend(
            [
                f"## {record['id']}",
                "",
                "### User",
                "",
                record["messages"][1]["content"],
                "",
                "### Assistant",
                "",
                record["messages"][2]["content"],
                "",
            ]
        )
    return "\n".join(chunks).rstrip() + "\n"


def make_manifest(records: list[dict], train: list[dict], evals: list[dict]) -> str:
    sources = []
    for pattern in SOURCE_GLOBS:
        for path in sorted(ROOT.glob(pattern)):
            if not path.is_file():
                continue
            data = path.read_bytes()
            sources.append(
                {
                    "path": rel(path),
                    "bytes": len(data),
                    "sha256": hashlib.sha256(data).hexdigest(),
                }
            )
    manifest = {
        "format_version": 1,
        "generator": "TRAIN_IA_MODEL/build.py",
        "system_prompt": SYSTEM_PROMPT,
        "counts": {
            "total": len(records),
            "train": len(train),
            "eval": len(evals),
            "categories": {
                category: sum(1 for r in records if r["category"] == category)
                for category in sorted({r["category"] for r in records})
            },
        },
        "formats": {
            "chat.jsonl": "OpenAI-style chat messages",
            "train.jsonl": "90% split, chat messages",
            "eval.jsonl": "10% deterministic split, chat messages",
            "alpaca.jsonl": "instruction/input/output JSONL",
            "concise.jsonl": "short syntax-focused chat records",
            "moh_gpt.jsonl": "concise messages-only JSONL for the moh-gpt train.sh pipeline",
            "plain.txt": "plain text for continued pretraining",
        },
        "sources": sources,
    }
    return json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"


def make_dataset_card(records: list[dict], train: list[dict], evals: list[dict]) -> str:
    categories = {
        category: sum(1 for r in records if r["category"] == category)
        for category in sorted({r["category"] for r in records})
    }
    rows = "\n".join(f"| `{name}` | {count} |" for name, count in categories.items())
    return f"""# RedBunny Training Dataset Card

Generated from the RedBunny repository by `TRAIN_IA_MODEL/build.py`.

Hugging Face dataset: https://huggingface.co/datasets/mohamedboukerche55/redbunny-training-data

## Counts

| Split | Examples |
|---|---:|
| Total | {len(records)} |
| Train | {len(train)} |
| Eval | {len(evals)} |

## Categories

| Category | Examples |
|---|---:|
{rows}

## Formats

- `chat.jsonl` - OpenAI-style `messages` records with a RedBunny system prompt.
- `train.jsonl` - deterministic 90% training split.
- `eval.jsonl` - deterministic 10% evaluation split.
- `alpaca.jsonl` - `instruction`, `input`, `output` records.
- `concise.jsonl` - short syntax-focused chat records.
- `moh_gpt.jsonl` - concise messages-only records compatible with `moh-gpt/train.sh`.
- `plain.txt` - plain text corpus for continued pretraining.

## Regenerate

```bash
python3 TRAIN_IA_MODEL/build.py
```

The generator scans `docs/`, `examples/`, `lib/`, `tests/`, `README.md` and the
curated `TRAIN_IA_MODEL/seed.jsonl`. Add a file or a seed record, rerun the
build, and the dataset is updated.
"""


def render() -> dict[Path, str]:
    records = build_records()
    evals = [r for r in records if is_eval(r)]
    if not evals and len(records) > 1:
        evals = [records[-1]]
    eval_ids = {r["id"] for r in evals}
    train = [r for r in records if r["id"] not in eval_ids]

    chat_rows = [
        {k: r[k] for k in ("id", "category", "source", "messages")} for r in records
    ]
    train_rows = [
        {k: r[k] for k in ("id", "category", "source", "messages")} for r in train
    ]
    eval_rows = [
        {k: r[k] for k in ("id", "category", "source", "messages")} for r in evals
    ]
    concise = make_concise(records)
    concise_rows = [
        {k: r[k] for k in ("id", "category", "source", "messages")} for r in concise
    ]
    alpaca_rows = [
        {k: r[k] for k in ("id", "category", "source", "instruction", "input", "output")}
        for r in records
    ]
    moh_gpt_rows = [{"messages": r["messages"]} for r in concise]

    files = {
        OUT / "chat.jsonl": jsonl(chat_rows),
        OUT / "train.jsonl": jsonl(train_rows),
        OUT / "eval.jsonl": jsonl(eval_rows),
        OUT / "alpaca.jsonl": jsonl(alpaca_rows),
        OUT / "concise.jsonl": jsonl(concise_rows),
        OUT / "moh_gpt.jsonl": jsonl(moh_gpt_rows),
        OUT / "plain.txt": make_plain_text(records),
        OUT / "manifest.json": make_manifest(records, train, evals),
        OUT / "dataset_card.md": make_dataset_card(records, train, evals),
    }
    return files


def jsonl(rows: list[dict]) -> str:
    return "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit non-zero if generated files are stale instead of writing them",
    )
    args = parser.parse_args()

    files = render()
    OUT.mkdir(parents=True, exist_ok=True)
    stale = []
    for path, content in files.items():
        current = path.read_text(encoding="utf-8") if path.exists() else None
        if current != content:
            stale.append(path)

    if args.check:
        if stale:
            print("Training data is stale:")
            for path in stale:
                print(f"  {rel(path)}")
            print("Run: python3 TRAIN_IA_MODEL/build.py")
            return 1
        print("Training data is up to date.")
        return 0

    for path, content in files.items():
        path.write_text(content, encoding="utf-8")
    manifest = json.loads(files[OUT / "manifest.json"])
    concise_count = len(
        files[OUT / "concise.jsonl"].splitlines()
    )
    print(
        "Wrote {total} examples ({train} train, {eval} eval, {concise} concise) to {out}".format(
            total=manifest["counts"]["total"],
            train=manifest["counts"]["train"],
            eval=manifest["counts"]["eval"],
            concise=concise_count,
            out=rel(OUT),
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
