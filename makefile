TARGET = Server
CC = @-g++
CFLAGS = -std=c++17 -I$(INC_DIR) 
LDFLAGS = -lboost_system -lpthread
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEP_DIR)/$*.Td
# The postcompile step is to update timestamps of the object file, so we don't always rebuild
POSTCOMPILE = @-mv -f $(DEP_DIR)/$*.Td $(DEP_DIR)/$*.d && touch $@

SRC_DIR := src
INC_DIR := include
DEP_DIR := dep
OBJ_DIR := obj

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(subst $(SRC_DIR), $(OBJ_DIR), $(SRCS:.cpp=.o))
DEPS := $(subst $(SRC_DIR), $(DEP_DIR), $(SRCS:.cpp=.d))

.PHONY: default
default: all

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	@-echo "Building $@"
	$(CC) $(LDFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(DEP_DIR)/%.d | $(OBJ_DIR) $(DEP_DIR)
	@-echo "Building $<"
	$(CC) $(DEPFLAGS) $(CFLAGS) -c $< -o $@
	$(POSTCOMPILE)

$(OBJ_DIR):
	-@mkdir -p $@

$(DEP_DIR):
	-@mkdir -p $@

.PHONY: clean
clean:
	-@rm -rf $(OBJ_DIR)
	-@rm -f $(TARGET)

.PHONY: cleanall
cleanall: clean
	-@rm -rf $(DEP_DIR)

$(DEPS):

include $(wildcard $(DEPS))


