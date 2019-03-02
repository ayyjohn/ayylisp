; atoms
(def {nil} {})
(def {true} 1)
(def {false} 0)

; function definitions
(def {fun} (\ {f b} {
  def (head f) (\ (tail f) b)
}))
