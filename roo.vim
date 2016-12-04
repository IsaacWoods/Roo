if exists("b:current_syntax")
  finish
endif

syntax keyword rooKeyword type fn import break return if else
highlight link rooKeyword Keyword

syntax keyword rooBools true false
highlight link rooBools Boolean

syntax match rooNumber '\d\+' contained display
syntax match rooNumber '[-+]\d\+' contained display
syntax match rooNumber '\d\+\.\d*' contained display
syntax match rooNumber '[-+]\d\+\.\d*' contained display
highlight link rooNumber Number

syntax region rooString start='"' end='"' contained
highlight link rooString Constant

let b:current_syntax="roo"
