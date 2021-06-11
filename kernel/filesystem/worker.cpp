#include "exfat.hpp"
#include "lock_guard.hpp"
#include "process/include/core_state.hpp"
#include "process/include/process.hpp"
#include "dev/include/disk_interface.hpp"

using namespace UOS;

void exfat::thread_pool::launch(exfat* fs,unsigned count){
	if (stopping)
		return;
	lock_add<dword>(&worker_count,count);
	this_core core;
	auto this_proceess = core.this_thread()->get_process();
	qword args[4] = { reinterpret_cast<qword>(fs) };
	while(count--){
		auto th = this_proceess->spawn(exfat::thread_worker,args);
		assert(th);
	}
}

void exfat::thread_pool::stop(void){
	stopping = true;
	do{
		barrier.signal_one();
		barrier.wait(1000*1000);
	}while(worker_count);
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
		if (stopping){
			assert(worker_count);
			lock_sub(&worker_count,(dword)1);
			barrier.signal_one();
			return nullptr;
		}
		else{
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
		if (!f){
			//stopping
			this_core core;
			thread::kill(core.this_thread());
			bugcheck("thread_worker failed to exit");
		}
		// dispatch
		switch(f->command){
			case file::COMMAND_READ:
				self->worker_read(f);
				break;
			case file::COMMAND_WRITE:
				self->worker_write(f);
				break;
			case file::COMMAND_LIST:
				self->worker_list(f);
			default:
				lock_or(&f->iostate,(word)OP_FAILURE);
				//f->iostate |= OP_FAILURE;
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
	assert(f->command == file::COMMAND_READ && !f->instance->is_folder());
	auto dst = reinterpret_cast<qword>(f->buffer);
	dword len = 0;
	if (dst == 0 || f->length == 0){
		f->length = 0;
		lock_or(&f->iostate,(word)MEM_FAILURE);
		//f->iostate |= MEM_FAILURE;
		return;
	}
	lock_guard<file_instance> guard(*f->instance,rwlock::SHARED);
	const auto file_size = f->instance->get_size();
	const auto valid_size = f->instance->get_valid_size();
	assert(file_size >= valid_size);
	while(len < f->length){
		if (f->offset >= file_size){
			lock_or(&f->iostate,(word)EOF_FAILURE);
			//f->iostate |= EOF_FAILURE;
			break;
		}
		if (f->offset >= valid_size){
			auto transfer_size = min<qword>(file_size - valid_size,f->length - len);
			auto transferred = f->host->vspace->zero(dst,transfer_size);
			len += transferred;
			if (transferred != transfer_size){
				lock_or(&f->iostate,(word)MEM_FAILURE);
				//f->iostate |= MEM_FAILURE;
				break;
			}
			continue;
		}
		auto lba = f->instance->get_lba(f->offset);
		if (!lba){
			lock_or(&f->iostate,(word)FS_FAILURE);
			//f->iostate |= FS_FAILURE;
			break;
		}
		auto count = dm.count(lba);
		auto off = f->offset & SECTOR_MASK;
		auto transfer_size = min<qword>(count*SECTOR_SIZE - off, f->length - len);
		transfer_size = min(transfer_size, valid_size - f->offset);
		transfer_size = min(transfer_size, cluster_size() - f->offset % cluster_size());
		assert(transfer_size);

		count = align_up(transfer_size,SECTOR_SIZE)/SECTOR_SIZE;
		auto block = dm.get(lba,count,false);
		if (!block){
			lock_or(&f->iostate,(word)MEDIA_FAILURE);
			//f->iostate |= MEDIA_FAILURE;
			break;
		}
		auto sor = static_cast<const byte*>(block->data(lba)) + off;
		auto transferred = f->host->vspace->write(dst,sor,transfer_size);

		dm.relax(block);
		f->offset += transferred;
		dst += transferred;
		len += transferred;
		if (transferred != transfer_size){
			lock_or(&f->iostate,(word)MEM_FAILURE);
			//f->iostate |= MEM_FAILURE;
			break;
		}
	}
	// set total transfer size
	f->length = len;
}

void exfat::worker_list(file* f){
	assert(f->command == file::COMMAND_LIST && f->instance->is_folder());
	auto dst = reinterpret_cast<qword>(f->buffer);

	if (dst == 0 || f->length == 0){
		f->length = 0;
		lock_or(&f->iostate,(word)MEM_FAILURE);
		//f->iostate |= MEM_FAILURE;
		return;
	}
	if (f->offset & 0x1F){
		f->length = 0;
		lock_or(&f->iostate,(word)OP_FAILURE);
		//f->iostate |= OP_FAILURE;
		return;
	}
	lock_guard<file_instance> guard(*f->instance,rwlock::SHARED);
	//auto file_size = f->instance->get_size();
	folder_instance::reader rec(*static_cast<folder_instance*>(f->instance),f->offset/exfat::record_size);

	word stat = rec.step(ALLOW_FILE | ALLOW_FOLDER);
	f->offset = rec.get_index() * exfat::record_size;
	if (stat){
		lock_or(&f->iostate,stat);
		f->length = 0;
		return;
	}
	auto file_name = rec.get_name();
	dword sz = file_name.size();
	if (f->length < sz){
		lock_or(&f->iostate,(word)MEM_FAILURE);
		//f->iostate |= MEM_FAILURE;
		sz = f->length;
	}

	f->length = f->host->vspace->write(dst,file_name.c_str(),sz);
	if (f->length != sz){
		lock_or(&f->iostate,(word)MEM_FAILURE);
		//f->iostate |= MEM_FAILURE;
	}
	return;
}

void exfat::worker_write(file* f){
	assert(f->command == file::COMMAND_WRITE && !f->instance->is_folder());
	auto sor = reinterpret_cast<qword>(f->buffer);
	dword len = 0;
	if (sor == 0 || f->length == 0){
		f->length = 0;
		lock_or(&f->iostate,(word)MEM_FAILURE);
		//f->iostate |= MEM_FAILURE;
		return;
	}
	lock_guard<file_instance> guard(*f->instance,rwlock::EXCLUSIVE);
	auto file_size = f->instance->get_size();
	auto valid_size = f->instance->get_valid_size();
	assert(file_size >= valid_size);
	while(len < f->length){
		//const auto top_size = align_up(valid_size,cluster_size());
		const auto off = f->offset & SECTOR_MASK;

		auto lba = f->instance->get_lba(f->offset,true);
		if (!lba){
			lock_or(&f->iostate,(word)FS_FAILURE);
			//f->iostate |= FS_FAILURE;
			break;
		}
		auto cnt = dm.count(lba);
		auto transfer_size = min<qword>(cnt*SECTOR_SIZE - off, f->length - len);
		transfer_size = min(transfer_size, cluster_size() - f->offset % cluster_size());

		assert(transfer_size && lba);
		auto count = align_up(transfer_size,SECTOR_SIZE)/SECTOR_SIZE;
		bool overwrite = (off == 0 && count*SECTOR_SIZE == transfer_size);
		auto block = dm.get(lba,count,overwrite);
		if (!block){
			lock_or(&f->iostate,(word)MEDIA_FAILURE);
			//f->iostate |= MEDIA_FAILURE;
			break;
		}
		if (!overwrite)
			dm.upgrade(block,lba,count);
		auto dst = static_cast<byte*>(block->data(lba)) + off;
		auto transferred = f->host->vspace->read(sor,dst,transfer_size);

		dm.relax(block);
		f->offset += transferred;
		sor += transferred;
		len += transferred;
		valid_size = max(valid_size,f->offset);
		file_size = max(file_size,valid_size);
		if (transferred != transfer_size){
			lock_or(&f->iostate,(word)MEM_FAILURE);
			//f->iostate |= MEM_FAILURE;
			break;
		}
	}
	// set total transfer size & change file size
	f->instance->set_size(valid_size,file_size);
	f->length = len;
}