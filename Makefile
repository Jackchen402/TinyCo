.PHONY: all clean
MKDIR = mkdir
RM = rm
FLAGS = -lpthread -l dl -g
RMFLAGS = -rf
CC = g++

DIR_OBJS = objs
DIR_EXES = exes


DIR_COROUTINE = coroutine
DIR_SCHEDULE = schedule
DIR_EPOLLER = epoller
DIR_HOOK = hook

DIRS = $(DIR_OBJS) $(DIR_EXES) $(DIR_OBJS)/$(DIR_COROUTINE) $(DIR_OBJS)/$(DIR_SCHEDULE) $(DIR_OBJS)/$(DIR_EPOLLER) $(DIR_OBJS)/$(DIR_HOOK) 

EXE = test
EXE := $(addprefix $(DIR_EXES)/, $(EXE))
SRCCUR = $(wildcard *.cpp) 
SRC_COR = $(wildcard $(DIR_COROUTINE)/*.cpp)
SRC_SCHE = $(wildcard $(DIR_SCHEDULE)/*.cpp)
SRC_EPOLL = $(wildcard $(DIR_EPOLLER)/*.cpp)
SRC_HOOK = $(wildcard $(DIR_HOOK)/*.cpp)

OBJ_CUR = $(patsubst %.cpp,%.o,$(SRCCUR))
OBJ_COR = $(patsubst %.cpp,%.o,$(SRC_COR))
OBJ_SCHE = $(patsubst %.cpp,%.o,$(SRC_SCHE))
OBJ_EPOLL = $(patsubst %.cpp,%.o,$(SRC_EPOLL)) 
OBJ_HOOK = $(patsubst %.cpp,%.o,$(HOOK))

OBJ_CUR := $(addprefix $(DIR_OBJS)/, $(OBJ_CUR))
OBJ_COR := $(addprefix $(DIR_OBJS)/, $(OBJ_COR))
OBJ_SCHE := $(addprefix $(DIR_OBJS)/, $(OBJ_SCHE))
OBJ_EPOLL := $(addprefix $(DIR_OBJS)/, $(OBJ_EPOLL))
OBJ_HOOK := $(addprefix $(DIR_OBJS)/, $(OBJ_HOOK))

OBJS = $(OBJ_COR) $(OBJ_CUR) $(OBJ_SCHE) $(OBJ_EPOLL) $(OBJ_HOOK) $(OBJ_CUR)



all: $(DIRS) $(EXE)

$(DIRS):
	$(MKDIR) $@


$(EXE): $(OBJS)
	$(CC) $^ -o $@ $(FLAGS)
$(OBJ_COR) : $(SRC_COR)
	$(CC) -c $^ -o $@
$(OBJ_SCHE) : $(SRC_SCHE)
	$(CC) -c $^ -o $@
$(OBJ_EPOLL) : $(SRC_EPOLL)
	$(CC) -c $^ -o $@
$(OBJ_HOOK) : $(SRC_HOOK)
	$(CC) -c $^ -o $@
$(OBJ_CUR) : $(SRCCUR)
	$(CC) -c $^ -o $@


clean:
	$(RM) $(RMFLAGS) $(DIRS)