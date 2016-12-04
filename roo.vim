if exists("b:current_syntax")
  finish
endif

syntax keyword rooKeyword type fn import break return if else
highlight link rooKeyword Keyword

syntax keyword bools true false
highlight link bools Boolean

syntax match number '\d\+' contained display
syntax match number '[-+]\d\+' contained display
syntax match number '\d\+\.\d*' contained display
syntax match number '[-+]\d\+\.\d*' contained display
highlight link number Number

syntax region string start='"' end='"' contained
highlight link string Constant

let b:current_syntax="roo"
