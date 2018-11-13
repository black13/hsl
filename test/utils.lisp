(defun mapcar (f l)
  (if (null l)
      nil
    (cons (funcall f (car l))
          (mapcar f (cdr l)))))

;(defspecial cond (&rest args)
;  (if (atom args)
;      args
;    (let ((clause (car args)))
;      (if (eval (car clause))
;          (eval (cons 'progn (cdr clause)))
;        (cond (cdr args))))))


(defun length (l)
  (if (atom l)
      0
    (+ 1 (length (cdr l)))))

(defun make-list (length atom)
  (if (eqv length 0)
      '()
    (cons atom (make-list (- length 1) atom))))

(defun fac (n)
  (if (zerop n)
      1
    (* n (fac (- n 1)))))

;(defspecial and (&rest args)
;  (print (list "and" args))
;  (let ((thisone (eval (car args))))
;    (if thisone
;        (if (atom (cdr args))
;            thisone
;          (and (cdr args))))))
