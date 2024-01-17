ROOTDIR 			:= $(realpath .)
ROOT_DIR      		:= $(ROOTDIR)/root
LOGS_DIR      		:= $(ROOTDIR)/logs
SRC_DIR      		:= $(ROOTDIR)/src
KERN_DIR    		:= $(SRC_DIR)/kern
KERN_CONF_DIR		:= $(KERN_DIR)/conf
KERNEL_CONF			:= DUMBVM
KERN_COMPILE_DIR	:= $(KERN_DIR)/compile/$(KERNEL_CONF)

# AUXILIARY TARGETS
PHONY: make-dirs
make-dirs:
	@mkdir -p $(LOGS_DIR)

# BUILD NEW VERSION FOR KERNEL
# USAGE: make build-release KERNEL_CONF=<DUMBVM/GENERAL/...>
PHONY: build-release
build-release: 
	@make clean
	bmake -C $(KERN_COMPILE_DIR) clean
	@make make-dirs
	@echo -e "./configure LOGS\n" > $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	cd $(SRC_DIR) && ./configure >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	@echo -e "\n\n\n\n\n\n\n./config LOGS\n" >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	cd $(KERN_CONF_DIR) && ./config $(KERNEL_CONF) >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	@echo -e "\n\n\n\n\n\n\nbmake depend LOGS\n" >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	bmake -C $(KERN_COMPILE_DIR) depend >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	@echo -e "\n\n\n\n\n\n\nbmake LOGS\n" >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	bmake -C $(KERN_COMPILE_DIR) >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1	
	@echo -e "\n\n\n\n\n\n\nbmake install LOGS\n" >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	bmake -C $(KERN_COMPILE_DIR) install >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1

# RUN OS161 W/O DEBUGGER
# USAGE: make run-release
PHONY: run-release
run-release:
	cd $(ROOT_DIR) && sys161 kernel

# RUN OS161 WITH DEBUGGER
# USAGE: make run-debug
PHONY: run-debug
run-debug:
	cd $(ROOT_DIR) && sys161 -w kernel



# CLEAN
# USAGE: make clean
PHONY: clean
clean:
	@rm -rf $(LOGS_DIR)/*
	