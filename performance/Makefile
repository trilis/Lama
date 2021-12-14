TESTS=$(sort $(basename $(wildcard *.lama)))

LAMAC=../src/lamac

.PHONY: check $(TESTS)

check: $(TESTS)

$(TESTS): %: %.lama
	@echo $@
	LAMA=../runtime $(LAMAC)  $< && `which time` -f "$@ (compiler)\t%U" ./$@
	`which time` -f "$@ (recursive SM)\t%U" $(LAMAC) -s $< < ../regression/test054.input
	`which time` -f "$@ (byterun)\t%U" sh -c "$(LAMAC) -b $< | ../byterun/byterun $@.bc"

clean:
	$(RM) test*.log *.s *~ $(TESTS) *.i
