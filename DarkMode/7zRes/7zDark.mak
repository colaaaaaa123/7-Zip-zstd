!IFDEF MAK_SINGLE_FILE

!IFDEF DARK_MODE_OBJS
$(DARK_MODE_OBJS): ../../../../DarkMode/lib/src/$(*B).cpp
	$(COMPL_O1) -DUNICODE -D_UNICODE -I../../../../DarkMode/7zRes -I../../../../DarkMode/lib/include
!ENDIF

!ELSE

{../../../../DarkMode/lib/src}.cpp{$O}.obj::
	$(CC) $(CFLAGS_O1) $< -DUNICODE -D_UNICODE -I../../../../DarkMode/7zRes -I../../../../DarkMode/lib/include

!ENDIF
