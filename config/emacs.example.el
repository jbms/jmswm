(make-variable-buffer-local 'frame-title-format)
(make-variable-buffer-local 'icon-title-format)

(setq my-major-modes-with-meaningful-directory
  '(magit-log-mode magit-status-mode dired-mode))

(defun my-frame-title ()
  (concat (buffer-name)
          (if (buffer-file-name)
              (concat " [" (file-name-directory (buffer-file-name)) "]")
            (if (memq major-mode my-major-modes-with-meaningful-directory)
                (concat " [" default-directory "]")))))

(setq-default frame-title-format
              '((:eval (my-frame-title))))
(setq-default icon-title-format frame-title-format)
