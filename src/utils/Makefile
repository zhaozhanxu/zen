#for APP CMDLINE
APP_CTL = $(APP)_ctl

APP_SRC += main.c
APP_SRC += cmd.c
APP_SRC += ipc.c

EXTRA_CFLAGS += -l readline

all:$(APP_CTL)
$(APP_CTL):$(APP_SRC)
	@$(CC) $^ -o $@ $(CFLAGS) $(EXTRA_CFLAGS)

clean:
	@rm -rf $(APP_CTL)
