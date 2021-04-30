#include "exfat.hpp"
#include "lock_guard.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "dev/include/disk_interface.hpp"

using namespace UOS;

void exfat::thread_pool::launch(exfat* fs,unsigned count){
	lock_add<dword>(&worker_count,count);
	this_core core;
	auto this_proceess = core.this_thread()->get_process();
	qword args[4] = { reinterpret_cast<qword>(fs) };
	while(count--){
		auto th = this_proceess->spawn(exfat::thread_worker,args);
		assert(th);
	}
}

void exfat::thread_pool::put(file* f){
	assert(f && f->command);
	f->acquire();
	f->host->acquire();
	f->next = nullptr;
	{
		lock_guard<rwlock> guard(objlock);
		if (request_list.tail){
			assert(request_list.head);
			assert(request_list.tail->next == nullptr);
			request_list.tail->next = f;
			request_list.tail = f;
		}
		else{
			assert(nullptr == request_list.head);
			request_list.head = request_list.tail = f;
		}
	}
	barrier.signal_one();
}

file* exfat::thread_pool::get(void){
	do{
		{
			lock_guard<rwlock> guard(objlock);
			auto f = request_list.head;
			if (f){
				assert(request_list.tail);
				if (request_list.tail == f){
					assert(f->next == nullptr);
					request_list.tail = nullptr;
				}
				else{
					assert(f->next);
				}
				request_list.head = f->next;
				f->next = nullptr;
				return f;
			}
		}
		auto reason = barrier.wait();
		assert(reason == PASSED || reason == NOTIFY);
	}while(true);
}

void exfat::thread_worker(qword arg,qword,qword,qword){
	auto self = reinterpret_cast<exfat*>(arg);

	while(true){
		auto f = self->workers.get();
		assert(f);
		// dispatch
		switch(f->command){
			case file::COMMAND_READ:
				self->worker_read(f);
				break;
			case file::COMMAND_WRITE:
				self->worker_write(f);
				break;
			default:
				f->iostate |= FAIL;
		}
		{
			interrupt_guard<spin_lock> guard(f->objlock);
			f->command = 0;
			guard.drop();
			f->notify();
		}
		f->host->relax();
		f->relax();
	}
}

void exfat::worker_read(file* f){
	assert(f->command == file::COMMAND_READ);
	auto dst = reinterpret_cast<qword>(f->buffer);
	dword len = 0;
	if (dst == 0 || f->length == 0 || f->iostate){
		f->length = 0;
		f->iostate |= FAIL;
		return;
	}
	lock_guard<file_instance> guard(*f->instance);
	auto file_size = f->instance->get_size();
	while(len < f->length){
		if (f->offset >= file_size){
			f->iostate |= EOF;
			break;
		}
		auto lba = f->instance->get_lba(f->offset);
		if (!lba){
			f->iostate |= FAIL;
			break;
		}
		auto count = dm.count(lba);
		auto off = f->offset & SECTOR_MASK;
		auto transfer_size = min<qword>(count*SECTOR_SIZE - off,f->length - len);
		transfer_size = min(transfer_size,file_size - f->offset);
		assert(transfer_size);

		count = align_up(transfer_size,SECTOR_SIZE)/SECTOR_SIZE;
		auto block = dm.get(lba,count,false);
		if (!block){
			//media failure
			f->iostate |= BAD;
			break;
		}
		auto sor = static_cast<byte*>(block->data(lba)) + off;
		auto transferred = f->host->vspace->write(dst,sor,transfer_size);

		dm.relax(block);
		f->offset += transferred;
		dst += transferred;
		len += transferred;
		if (transferred != transfer_size){
			f->iostate |= FAIL;
			break;
		}
	}
	// set total transfer size
	f->length = len;
}

void exfat::worker_write(file* f){
	bugcheck("FAT32::worker_write not implemented");
}