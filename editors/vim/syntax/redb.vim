" Vim/Neovim syntax file for RedBunny (.redb)
" Language: RedBunny
" Last Change: 2026-10-31

if exists("b:current_syntax")
  finish
endif

" Language keywords and declarations.
syn keyword redbKeyword main for in to downTo do doto return if else while break continue include
syn keyword redbType int float double bool str void array auto
syn keyword redbBoolean true false
syn keyword redbFunction write writeln writeformat size resize
syn match redbInclude "@include\>"

" Operators and punctuation.
syn match redbOperator /->\|=>\|==\|!=\|<=\|>=\|&&\|||\|[=+*%<>!\/-]/
syn match redbDelimiter /[(){}\[\],;.]/

" Numeric literals. Floats are defined after integers so the longer match wins.
syn match redbNumber /\<\d\+\>/
syn match redbFloat /\<\d\+\.\d\+\%([eE][+-]\?\d\+\)\?\>/
syn match redbFloat /\<\d\+[eE][+-]\?\d\+\>/

" Strings and comments.
syn region redbString start=+"+ skip=+\\\\\|\\"+ end=+"+ contains=@Spell
syn region redbComment start=/--/ end=/$/ contains=@Spell
syn region redbComment start=/-[*]/ end=/[*]-/ contains=@Spell

hi def link redbKeyword Keyword
hi def link redbType Type
hi def link redbBoolean Boolean
hi def link redbFunction Function
hi def link redbInclude PreProc
hi def link redbOperator Operator
hi def link redbDelimiter Delimiter
hi def link redbNumber Number
hi def link redbFloat Number
hi def link redbString String
hi def link redbComment Comment

let b:current_syntax = "redb"
