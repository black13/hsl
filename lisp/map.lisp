(defun map (fun list)
  (if (null list)
      nil
    (cons (fun (car list))
          (map fun (cdr list)))))
