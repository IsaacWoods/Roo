if exists("b:current_syntax")
  finish
endif

syntax keyword rooKeyword type fn import break return if else while mut operator
highlight link rooKeyword Keyword

syntax keyword rooBools true false
highlight link rooBools Boolean

syntax match rooNumber display contained '\d\+' containedin=ALL
highlight def link rooNumber Number

syntax region rooString start='"' end='"' contained
highlight link rooString Constant

let b:current_syntax="roo"
