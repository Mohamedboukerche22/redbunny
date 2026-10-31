# RedBunny for Vim and Neovim

Syntax highlighting, filetype detection, comments and indentation for `.redb`
files. The same files work in Vim 8+ and Neovim.

## Install

### Vim 8+ package

```bash
mkdir -p ~/.vim/pack/redb/start
ln -s /path/to/redb/editors/vim ~/.vim/pack/redb/start/redb
```

### Neovim package

```bash
mkdir -p ~/.local/share/nvim/site/pack/redb/start
ln -s /path/to/redb/editors/vim ~/.local/share/nvim/site/pack/redb/start/redb
```

### Add to `runtimepath` directly

```vim
set runtimepath+=/path/to/redb/editors/vim
filetype plugin indent on
syntax on
```

### lazy.nvim

```lua
{
  dir = "/path/to/redb/editors/vim",
  name = "redb",
}
```

## Verify

Open any `.redb` file and run:

```vim
:set filetype?
```

It should print `filetype=redb`.

Highlighted keywords include `main`, `for`, `in`, `to`, `downTo`, `do`, `doto`,
`return`, `if`, `else`, `while`, `break`, `continue`, `true`, `false`, the
built-in output functions, and the types `int`, `float`, `double`, `str` and
`void`. Comments use `--` for one line and `-* ... *-` for blocks.
