ROOTDIR 			:= $(realpath .)
ROOT_DIR      		:= $(ROOTDIR)/root
OSTREE				:= $(ROOT_DIR)
OLD_OSTREE          := $$(HOME)/os161/root
LOGS_DIR      		:= $(HOME)/logs
SRC_DIR      		:= $(ROOTDIR)/src
KERN_DIR    		:= $(SRC_DIR)/kern
KERN_CONF_DIR		:= $(KERN_DIR)/conf
KERNEL_CONF			:= DUMBVM
KERN_COMPILE_DIR	:= $(KERN_DIR)/compile/$(KERNEL_CONF)

# AUXILIARY TARGETS
PHONY: make-dirs
make-dirs:
	@mkdir -p $(LOGS_DIR)

PHONY: print-OSTREE
print-OSTREE:
	@echo $(OSTREE)


# CONFIGURE CUSTOM SRC TREE FOR BUILD
# USAGE: make configure-custom-src-tree
PHONY: configure-custom-src-tree
configure-custom-src-tree:
	cd $(SRC_DIR) && cp configure configure_bak && sed -i 's#$(OLD_OSTREE)#$(OSTREE)#' configure && ./configure && mv configure_bak configure

# BUILD NEW VERSION FOR KERNEL
# USAGE: make build-release KERNEL_CONF=<DUMBVM/GENERAL/...>
PHONY: build-release
build-release: 
	@make clean
	bmake -C $(KERN_COMPILE_DIR) clean
	@make make-dirs

	@echo -e "bmake clean LOGS\n" > $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	bmake -C $(KERN_COMPILE_DIR) clean >> $(LOGS_DIR)/build-$(KERNEL_CONF).log 2>&1
	
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
	