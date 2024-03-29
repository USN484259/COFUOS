#include "ide.hpp"
#include "pci.hpp"
#include "apic.hpp"
#include "cpu.hpp"
#include "memory/include/pm.hpp"
#include "memory/include/vm.hpp"
#include "process/include/thread.hpp"
#include "lock_guard.hpp"
#include "intrinsics.hpp"

using namespace UOS;

/*	references:
https://wiki.osdev.org/ATA_PIO_Mode
https://wiki.osdev.org/ATA/ATAPI_using_DMA
https://wiki.osdev.org/PCI_IDE_Controller
*/

struct PRD{
	dword phy_addr;
	word size;
	word : 15;
	word last : 1;
} __attribute__((packed));
static_assert(sizeof(PRD) == 8,"PRD size mismatch");

static volatile PRD* const prdt = (volatile PRD*)HIGHADDR(PRDT_PBASE);

IDE::IDE(void) : port_base{0x1F0,0x3F4,0x170,0x374},irq_vector{APIC::IRQ_IDE_PRI,APIC::IRQ_IDE_SEC},sync(1){
	auto dev = pci.find(1,1);
	if (dev == nullptr)
		bugcheck("IDE controller not present");
	pci_ref = dev;

	byte mode = dev->classify >> 8;
	if (0 == (mode & 0x80))
		bugcheck("IDE controller no DMA support");
	if (mode & 5){
		bugcheck("IDE native mode not supported");
	}
	dword bar4 = pci.read(dev->func,0x20);
	if (0 == (bar4 & 1) || bar4 >> 16)
		bugcheck("invalid DMA base %x",(qword)bar4);
	dma_base = bar4 & 0xFFFC;

	byte drive_mask = (in_byte(port_base[0] + 6) & 0x10) ? 0x40 : 0x20;

	byte stat = in_byte(dma_base + 2);
	if (0 == (stat & drive_mask) || (stat & 3))
		bugcheck("DMA not usable %x",(qword)stat);
	out_byte(dma_base + 2,4);	//clear interrupt flag

	zeromemory((void*)prdt,sizeof(PRD));
	out_dword(dma_base + 4,PRDT_PBASE);

	apic.set(irq_vector[0],on_irq,this);
	reset();
	dbgprint("BusMaster IDE initialized");
}

void IDE::reset(void){
	sync.wait();
	interrupt_guard<void> ig;
	byte sel = in_byte(port_base[0] + 6) & 0x10;
	out_byte(port_base[1] + 2,4);
	delay_us(8);
	out_byte(port_base[1] + 2,0);
	out_byte(port_base[0] + 6,sel);
	delay_us();
	sync.signal();
}

bool IDE::command(MODE mode,qword lba,dword pa,word size){
	if ((pa & SECTOR_MASK) || !size || (size & SECTOR_MASK))
		return false;
	if (lba & ~0x0FFFFFFF){
		dbgprint("LBA48 not supported: %x",lba);
		return false;
	}
	/*
		DMA command bit 3
		0 => read from memory (aka write disk)
		1 => write to memory (aka read disk)

		IDE command
		0xC8 => read DMA LBA28
		0x25 => read DMA LBA48
		0xCA => write DMA LBA28
		0x35 => write DMA LBA48
	*/
	byte dma_cmd;
	byte ide_cmd;
	switch(mode){
		case READ:
			dma_cmd = 8;
			ide_cmd = 0xC8;
			break;
		case WRITE:
			dma_cmd = 0;
			ide_cmd = 0xCA;
			dbgprint("IDE write @ LBA %x,0x%x",lba,size);
			break;
		default:
			bugcheck("unknown IDE operation %x",(qword)mode);
	}
	sync.wait();
	do{
		{
			interrupt_guard<void> ig;
			if (in_byte(dma_base + 0) & 1)	//DMA active
				break;
			if (in_byte(dma_base + 2) & 7)	//interrupt | fail | active
				break;
			byte sel = (in_byte(port_base[0] + 6) & 0x10) ? 0xF0 : 0xE0;

			out_byte(dma_base + 0,dma_cmd);
			prdt->phy_addr = pa;
			prdt->size = size;
			prdt->last = 1;

			ev.reset();

			out_byte(port_base[0] + 6,(0x0F & (lba >> 24)) | sel);
			delay_us();
			out_byte(port_base[0] + 2,size/SECTOR_SIZE);
			out_byte(port_base[0] + 3,lba);
			out_byte(port_base[0] + 4,lba >> 8);
			out_byte(port_base[0] + 5,lba >> 16);
			out_byte(port_base[0] + 7,ide_cmd);
			out_byte(dma_base + 0,1 | dma_cmd);	//enable DMA
		}
		switch(ev.wait(4*1000*1000)){
			case TIMEOUT:
				//abort DMA operation
				out_byte(dma_base + 0,dma_cmd);
				dbgprint("IDE timeout");
				//fall through
			case PASSED:
			case NOTIFY:
			{
				interrupt_guard<void> ig;
				bool result = true;
				byte dma_stat = in_byte(dma_base + 2);
				byte dev_stat = in_byte(port_base[0] + 7);
				out_byte(dma_base + 2,6);	//clear interrupt & err
				if (dev_stat & 1)
					result = false;
				if (dma_stat & 2)
					result = false;
				if (0 == (dma_stat & 4))
					result = false;
				if (!result){
					byte errcode = in_byte(port_base[0] + 1);
					word pci_stat = pci.read(pci_ref->func,4) >> 16;
					dbgprint("IDE failed: %x,%x,%x,%x",\
						(qword)dma_stat,(qword)dev_stat,\
						(qword)errcode,(qword)pci_stat);
				}
				sync.signal();
				return result;
			}
			default:
				bugcheck("IDE event corrupted");
		}
	}while(false);
	bugcheck("invalid IDE state");
}

bool IDE::on_irq(byte irq,void* ptr){
	auto self = (IDE*)ptr;
	byte stat = in_byte(self->dma_base + 2);
	if (0 == (stat & 4))	//not this device
		return false;
	byte cmd = in_byte(self->dma_base);
	cmd &= 0xFE;	//halt the DMA
	out_byte(self->dma_base + 0,cmd);
	self->ev.signal_all();
	return true;
}
