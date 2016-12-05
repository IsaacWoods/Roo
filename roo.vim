if exists("b:current_syntax")
  finish
endif

syntax keyword rooKeyword type fn import break return if else
highlight link rooKeyword Keyword

syntax keyword rooBools true false
highlight link rooBools Boolean

syntax match rooNumber display contained "\d\+\(u\=l\{0,2}\|ll\=u\)\>"
highlight def link rooNumber Number

syntax region rooString start='"' end='"' contained
highlight link rooString Constant

let b:current_syntax="roo"
