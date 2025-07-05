include src/Makefile

TARGET := program
TOOL := tool

.PHONY: clean all src TARGET TOOL

all:: clean src $(TARGET) $(TOOL)
	
src:
	$(MAKE) -C src
	
$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $^ src/*.o

$(TOOL): tool.c
	$(CC) $(CFLAGS) -o $@ $^ src/*.o

clean::
	$(MAKE) clean -C src
	rm -f $(TARGET)
	rm -f $(TOOL)
