TARGETS = rds-saa6588

all: $(TARGETS)

-include *.d

rds-saa6588: rds-saa6588.o
	$(CC) $(LDFLAGS) -o $@ $^

install:

include ../../Make.rules
