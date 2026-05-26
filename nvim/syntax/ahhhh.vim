if exists("b:current_syntax")
  finish
endif

syntax keyword ahhhhKeyword enum struct fn export var const return if else switch case default for in while br fw mk rm self
syntax keyword ahhhhLiteral true false null
syntax keyword ahhhhType f64 string bool void

syntax keyword ahhhhBuiltin print log input exit sqrt abs pow ln exp log10 atan2 sin cos tan floor ceil clock append pop

syntax match ahhhhComment "//.*$"
syntax region ahhhhString start=+"+ skip=+\\\\\|\\"+ end=+"+
syntax match ahhhhNumber /\v<\d+(\.\d+)?>/

syntax region ahhhhAnnotation start=/@(/ end=/)/ contains=ahhhhAnnotationPath
syntax match ahhhhAnnotationPath /[^)]\+/ contained

syntax match ahhhhRange /\.\./
syntax match ahhhhColor /\v\.[a-z_][a-z0-9_]*/
syntax match ahhhhType /\v<[A-Z][A-Za-z0-9_]*>/
syntax match ahhhhFunction /\v<[a-z_][a-z0-9_]*\ze\s*\(/
syntax match ahhhhOperator "==\|!=\|<=\|>=\|[=+\-*/%<>!&]"
syntax match ahhhhDelimiter /[(){}\[\],:]/

highlight default link ahhhhKeyword Keyword
highlight default link ahhhhLiteral Constant
highlight default link ahhhhType Type
highlight default link ahhhhBuiltin Special
highlight default link ahhhhComment Comment
highlight default link ahhhhString String
highlight default link ahhhhNumber Number
highlight default link ahhhhAnnotation PreProc
highlight default link ahhhhAnnotationPath String
highlight default link ahhhhRange Operator
highlight default link ahhhhColor Special
highlight default link ahhhhFunction Function
highlight default link ahhhhOperator Operator
highlight default link ahhhhDelimiter Delimiter

let b:current_syntax = "ahhhhh"
