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
(fun {first l}  { eval (head l) })
(fun {second l} { eval (head (tail l)) })
(fun {third l}  { eval (head (tail (tail l))) })

; get the length of a list
(fun {len l} {
  if (== l nil)
    {0}
    {+ 1 (len (tail l))}
})

; get nth item in a list
(fun {nth n l} {
  if (== n 0)
    {first l}
    {nth (- n 1) (tail l)}
})

; get the last item in a list
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

; checks for element x's inclusion in a list
(fun {contains x l} {
  if (== l nil)
    {false}
    {if (== x (first l)) {true} {contains x (tail l)}}
})

; apply function to each item in a list
(fun {map f l} {
  if (== l nil)
    {nil}
    {join (list (f (first l))) (map f (tail l))}
})

; filter out items that don't return true from a list
(fun {filter f l} {
  if (== l nil)
    {nil}
    {join (if (f (first l)) {head l} {nil}) (filter f (tail l))}
})

; fold items in a list into an accumulator
(fun {foldl f z l} {
  if (== l nil)
    {z}
    {foldl f (f z (first l)) (tail l)}
})

; calculate the sum of all numbers in a list
(fun {sum l} {foldl + 0 l})

; calculate the product of all numbers in a list
(fun {product l} {foldl * 1 l})

; for 0+ pairs, evaluate each and for the first true, evaluate the second
(fun {select & cs} {
  if (== cs nil)
    {error "no selection found"}
    {if (first (first cs)) {second (first cs)} {unpack select (tail cs)}}
})

; default case for case-switch and select statements
(def {otherwise} true)

; example select; get the suffix for a day of the month by index
(fun {month-day-suffix i} {
  select
  {(== i 0)  "st"}
  {(== i 1)  "nd"}
  {(== i 2)  "rd"}
  {otherwise "th"}
})

; case/switch statement, evaluate each possibility vs a constant input value
; and evaluate the match
(fun {case x & cs} {
  if (== cs nil)
  {error "no cases found"}
  {if (== x (first (first cs))) {second (first cs)} {
    unpack case (join (list x) (tail cs))}}
})

; example case/switch
(fun {day-name x} {
  case x
    {0 "Monday"}
    {1 "Tuesday"}
    {2 "Wednesday"}
    {3 "Thursday"}
    {4 "Friday"}
    {5 "Saturday"}
    {6 "Sunday"}
})
