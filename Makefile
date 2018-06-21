# define the C compiler to use
CC = gcc

#USE_WIRINGPI := 1

ifeq ($(USE_WIRINGPI),)
  sd_GPIO_C := sd_GPIO.c
else
  #WPI_LIB := -D USE_WIRINGPI -lwiringPi -lwiringPiDev
  WPI_LIB := -D USE_WIRINGPI -lwiringPi
endif

LIBS := $(WPI_LIB) -lm -lpthread
#LIBS := -lpthread -lwiringPi -lwiringPiDev -lm
#LIBS := -lpthread -lwebsockets

# debug of not
#$DBG = -g
$DBG =

# define any compile-time flags
GCCFLAGS = -Wall
#CFLAGS = -Wall -lpthread -lwiringPi -lwiringPiDev -lm -I. -I./minIni
CFLAGS = $(GCCFLAGS) -I. -I./minIni $(DBG) $(LIBS) -D MG_DISABLE_MD5 -D MG_DISABLE_HTTP_DIGEST_AUTH -D MG_DISABLE_MD5 -D MG_DISABLE_JSON_RPC
#CFLAGS = -Wextra -Wall -g -I./wiringPI


# define the C source files
SRCS = sprinkler.c utils.c config.c net_services.c json_messages.c zone_ctrl.c sd_cron.c mongoose.c minIni/minIni.c $(sd_GPIO_C)
TSRC = test.c config.c utils.c minIni/minIni.c

# define the C object files 
#
# This uses Suffix Replacement within a macro:
#   $(name:string1=string2)
#         For each word in 'name' replace 'string1' with 'string2'
# Below we are replacing the suffix .c of all words in the macro SRCS
# with the .o suffix
#
OBJS = $(SRCS:.c=.o)
TOBJ = $(TSRC:.c=.o)

# define the executable file 
MAIN = ./release/sprinklerd
TEST = ./release/testing

#
# The following part of the makefile is generic; it can be used to 
# build any executable just by changing the definitions above and by
# deleting dependencies appended to the file from 'make depend'
#

.PHONY: depend clean

all:    $(MAIN) 
  @echo: $(MAIN) have been compiled

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

test:    $(TEST)
  @echo: $(TEST) have been compiled

$(TEST): $(TOBJ)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TEST) $(TOBJ) $(LFLAGS) $(LIBS)
  
# this is a suffix replacement rule for building .o's from .c's
# it uses automatic variables $<: the name of the prerequisite of
# the rule(a .c file) and $@: the name of the target of the rule (a .o file) 
# (see the gnu make manual section about automatic variables)
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) *.o *~ $(MAIN) $(MAIN_U) $(TEST)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

install: $(MAIN)
	./release/install.sh
