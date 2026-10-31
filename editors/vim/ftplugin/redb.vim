" Vim/Neovim filetype settings for RedBunny (.redb)
if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

setlocal commentstring=--\ %s
setlocal comments=:--
setlocal suffixesadd=.redb

let b:undo_ftplugin = "setlocal commentstring< comments< suffixesadd<"
