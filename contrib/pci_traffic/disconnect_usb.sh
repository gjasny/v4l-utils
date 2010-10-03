#!/bin/bash

for i in 0 1 2 3 4 5 6 7; do
	echo "0000:00:1d.$i" >/sys/bus/pci/drivers/ehci_hcd/unbind
	echo "0000:00:1d.$i" >/sys/bus/pci/drivers/uhci_hcd/unbind
	echo "0000:00:1a.$i" >/sys/bus/pci/drivers/ehci_hcd/unbind
	echo "0000:00:1a.$i" >/sys/bus/pci/drivers/uhci_hcd/unbind
	echo "0000:00:1d.$i" >/sys/bus/pci/drivers/pci-stub/bind
done
