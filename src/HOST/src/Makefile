# -------------------------------------------------------------------------
#   __  __       _         __ _ _
#  |  \/  | __ _| | _____ / _(_) | ___
#  | |\/| |/ _` | |/ / _ \ |_| | |/ _ \
#  | |  | | (_| |   <  __/  _| | |  __/
#  |_|  |_|\__,_|_|\_\___|_| |_|_|\___|
#
# -------------------------------------------------------------------------

# -------------------------------------------------------------------------
#  Including standard rules
# -------------------------------------------------------------------------
PROROOT	= ..
include	../Rules.make

LDFLAGS	= -L$(COM_LIB) -L$(SQL_LIB)
#PROGLIB	= -lrt -lm -lpthread -lcmlb -ldbmb -lmysqlclient -lpq
PROGLIB	= -lrt -lm -lpthread -lcmlb -ldbmb -lmysqlclient -lpqmb -lpq
#PROGLIB	= -lrt -lm -lpthread -lmysqlclient -lcmlb -lpcap -ldbmb

PRODEFS = -g -Wall -O2 -D_REENTRANT -pedantic -Wpointer-arith\
	  -Wbad-function-cast -Wmissing-prototypes -Wmissing-declarations\
	  -Wstrict-prototypes -Wchar-subscripts -Wformat -Wnested-externs -Wcast-align -pthread

PROBOPT = -D__DEBUG__ -D__PGSQL__ -U__BPCTL__

COMFLAG	= -o $@

# -------------------------------------------------------------------------
#  File specifications
# -------------------------------------------------------------------------
OBJECTS = $(OBJ_DIR)/host_main.o\
	  $(OBJ_DIR)/host_conf.o\
	  $(OBJ_DIR)/host_task.o\
	  $(OBJ_DIR)/host_sock.o\
	  $(OBJ_DIR)/host_fifo.o\
	  $(OBJ_DIR)/host_debug.o


#OBJECTS = $(OBJ_DIR)/cfm_main.o\
	  $(OBJ_DIR)/cfm_conf.o\
	  $(OBJ_DIR)/cfm_task.o\
	  $(OBJ_DIR)/cfm_sock.o\
	  $(OBJ_DIR)/cfm_fifo.o\
	  $(OBJ_DIR)/cfm_sys.o\
	  $(OBJ_DIR)/cfm_msg.o\
	  $(OBJ_DIR)/cfm_debug.o


SOURCES = $(OBJECTS:.o=.c)

# -------------------------------------------------------------------------
#  Targets
# -------------------------------------------------------------------------
TARGET  = bpappHOST


all: Flags $(BIN_DIR)/$(TARGET)
$(BIN_DIR)/$(TARGET): $(OBJECTS)
	@$(CC) $(LDFLAGS) $(COMFLAG) $(OBJECTS) $(PROGLIB)
	@$(CP) $(BIN_DIR)/$(TARGET) $(COM_BIN)
	@echo '        ($(COM_BIN)/$(TARGET)) Created'
	@echo '-----------------------------------------------------------------'

Flags:
	@echo '================================================================='
	@echo ' BPAPP Detail Control Server '
	@echo '================================================================='
	@echo ' < Compile Flags >'
	@echo '$(CC) -c $(PRODEFS) $(COMFLAG) $(INC) $(PROBOPT)'
	@echo ' < LD Flags >'
	@echo '$(CC) $(LDFLAGS) $(COMFLAG) $(OBJECTS) $(PROGLIB)'
	@echo '-----------------------------------------------------------------'

# -------------------------------------------------------------------------
#  Compile source file
# -------------------------------------------------------------------------
$(OBJ_DIR)/%.o: %.c
	@$(CC) -c $(PRODEFS) $(COMFLAG) $(INC) $(PROBOPT) $<
	@echo 'HOST $(CC) compile...... $< '

# -------------------------------------------------------------------------
#  Delete object, target and library file
# -------------------------------------------------------------------------
clean:
	$(RM) -f $(OBJECTS) $(BIN_DIR)/$(TARGET) $(BIN_DIR)/core
#	$(RM) -f $(OBJECTS) $(BIN_DIR)/$(TARGET) $(BIN_DIR)/core $(SRC_DIR)/tags

# -------------------------------------------------------------------------
#  Copy object, target file
# -------------------------------------------------------------------------
copy:
	$(CP) $(BIN_DIR)/$(TARGET) $(COM_BIN)

# -------------------------------------------------------------------------
#  Copy object, target file
# -------------------------------------------------------------------------
tag:
	ctags -R ./* ../hdr/* ../../CMLB/* ../../DBMB/* ../../PQMB/* ../../../inc/*

# -------------------------------------------------------------------------
#  End of Makefile
# -------------------------------------------------------------------------
