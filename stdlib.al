; atoms
(def {nil} {})
(def {true} 1)
(def {false} 0)

; function definitions
(def {fun} (\ {f b} {
  def (head f) (\ (tail f) b)
}))

; unpack list for function evaluation
(fun {unpack f l} {
  eval (join (list f) l)
})

; pack list for function evaluation
(fun {pack f & xs} {f xs})

; alias packing/currying
(def {curry} unpack)
(def {uncurry} pack)

; perform several things in sequence
(fun {do & l} {
  if (== l nil)
    {nil}
    {last l}
})

; open new scope
(fun {let b} {
  ((\ {_} b) ())
})

; logical operators
(fun {not x}   {- 1 x})
(fun {or x y}  {+ x y})
(fun {and x y} {* x y})

; miscellaneous
; apply a function to the arguments in reverse order
(fun {flip f a b} {f b a})
; evaluates arguments as an expression
(fun {ghost & xs} {eval xs})
; composes a function out of two other functions
; can be used with def to define functions in shorthand
(fun {comp f g x} {f (g x)})

; convenience methods for item access
(fun {first l} {eval (head l) })
(fun {second l} {eval (head (tail l)) })
(fun {third l} {eval (head (tail (tail l))) })

; get list length
(fun {len l} {
  if (== 1 nil)
    {0}
    {+ 1 (len (tail l))}
})

; get nth item in a list
(fun {nth n l} {
  if (== n 0)
    {first l}
    {nth (- n 1) (tail l)}
})

; get the last item in the list
(fun {last l} {nth (- (len l) 1) l})

; get the first n items from a list
(fun {take n l} {
  if (== n 0)
    {nil}
    {join (head l) (take (- n 1) (tail l))}
})

; drop the first n items from a list
(fun {drop n l} {
  if (== n 0)
    {l}
    {drop (- n 1) (tail l)}
})

; split a list at index n
(fun {split n l} {list (take n l) (drop n l)})

; checks for element x's inclusion in the list
(fun {contains? x l} {
  if (== l nil)
    {false}
    {if (== x (first l)) {true} {contains? x (tail l)}}
})
