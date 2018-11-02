// File author is Ítalo Lima Marconato Matias
//
// Created on October 28 of 2018, at 09:41 BRT
// Last edited on November 02 of 2018, at 12:59 BRT

#include <chicago/alloc.h>
#include <chicago/chfs.h>
#include <chicago/string.h>

static UIntPtr CHFsGoToOffset(PCHFsMountInfo info, PCHFsINode file, UIntPtr off) {
	if ((info == Null) || (file == Null) || (info->dev == Null) || (info->dev->read == Null)) {				// Let's do some null pointer checks first!
		return 0;
	} else if ((file->data_length == 0) || (off >= file->data_length)) {									// Too high offset?
		return 0;
	}
	
	UIntPtr lba = file->data_start;																			// Get the start lba
	UIntPtr cur = 0;
	
	if (lba == 0) {
		return 0;																							// Invalid...
	} else if (off == 0) {																					// Offset is 0?
		return lba;																							// Yes!
	}
	
	PUInt8 buf = (PUInt8)MemAllocate(512);																	// Alloc some space for reading the disk
	
	if (buf == Null) {
		return 0;																							// Failed...
	}
	
	while (cur < off) {																						// Let's go!
		if ((lba == 0) || (cur >= file->data_length)) {														// Well, we can't continue, as the chain ends here...
			MemFree((UIntPtr)buf);
			return 0;
		} else if (!FsReadFile(info->dev, lba * 512, 512, buf)) {											// Read this block
			MemFree((UIntPtr)buf);																			// Failed...
			return 0;
		}
		
		if (off - cur >= 508) {																				// Our data block is 508-bytes long (would be 512, but 4-bytes are reserved for the next chain block num), we all the 508 bytes?
			cur += 508;																						// Yes
		} else {
			cur += off - cur;																				// No
		}
		
		lba = *((PUInt32)buf);																				// Get the next block from the chain
	}
	
	return lba;
}

Boolean CHFsReadFile(PFsNode file, UIntPtr off, UIntPtr len, PUInt8 buf) {
	if ((file == Null) || (file->priv == Null) || (file->inode == 0) || (file->read == Null)) {				// Let's do some null pointer checks first!
		return False;
	} else if ((file->flags & FS_FLAG_FILE) != FS_FLAG_FILE) {												// We're trying to read raw bytes from an directory?
		return False;
	} else if (off >= file->length) {																		// For byte per byte read
		return False;
	}
	
	PCHFsMountInfo info = (PCHFsMountInfo)file->priv;
	
	if (info->dev->read == Null) {																			// We have the read function... right?
		return False;
	} else if ((info->dev->flags & FS_FLAG_FILE) != FS_FLAG_FILE) {											// It's a file?
		return False;
	}
	
	PCHFsINode inode = (PCHFsINode)MemAllocate(512);														// Alloc space for reading the inode
	
	if (inode == Null) {
		return False;																						// Failed...
	}
	
	if (!FsReadFile(info->dev, file->inode * 512, 512, (PUInt8)inode)) {									// Read the inode
		MemFree((UIntPtr)inode);
		return False;
	}
	
	UIntPtr lba = CHFsGoToOffset(info, inode, off);															// Get the start lba for off
	
	if (lba == 0) {
		MemFree((UIntPtr)inode);
		return False;
	}
	
	PUInt8 buff = (PUInt8)MemAllocate(512);																	// Alloc some memory for reading the disk
	UIntPtr end = len;
	UIntPtr cur = 0;
	
	if (buff == Null) {
		MemFree((UIntPtr)inode);																			// ...
		return False;
	} else if ((off + len) > file->length) {																// Let's calc the size that we're going to read
		end = file->length - off;
	}
	
	while (cur < end) {																						// And read the file!
		if (lba == 0) {																						// Well, we can't continue, as the chain ends here...
			MemFree((UIntPtr)buff);
			MemFree((UIntPtr)inode);
			return 0;
		} else if (!FsReadFile(info->dev, lba * 512, 512, buff)) {											// Read the sector
			MemFree((UIntPtr)buff);																			// Failed...
			MemFree((UIntPtr)inode);
			return False;
		}
		
		if (end - cur >= 508) {																				// Our data block is 508-bytes long (would be 512, but 4-bytes are reserved for the next chain block num), we all the 508 bytes?
			StrCopyMemory(buf + cur, buff + 4, 508);														// Yes
			cur += 508;
		} else {
			StrCopyMemory(buf + cur, buff + 4, end - cur);													// No, so only read what we need
			cur += end - cur;
		}
		
		lba = *((PUInt32)buff);																				// Get the next block from the chain
	}
	
	MemFree((UIntPtr)buff);
	MemFree((UIntPtr)inode);
	
	return True;
}

Boolean CHFsOpenFile(PFsNode node) {
	if (node == Null) {																						// Trying to open an null pointer?
		return False;																						// Yes (probably someone called this function directly... or the kernel have some fatal bug)
	} else {
		return True;
	}
}

Void CHFsCloseFile(PFsNode node) {
	if (node == Null) {																						// Null pointer?
		return;																								// Yes
	} else if (StrCompare(node->name, "\\")) {																// Root directory?
		return;																								// Yes, don't free it, it's important for us (only the umount function can free it)
	}
	
	MemFree((UIntPtr)node->name);																			// Let's free everything that we allocated!
	MemFree((UIntPtr)node);
}

PChar CHFsReadDirectoryEntry(PFsNode dir, UIntPtr entry) {
	if ((dir == Null) || (dir->priv == Null) || (dir->inode == 0)) {										// Let's do some null pointer checks first!
		return Null;
	} else if ((dir->flags & FS_FLAG_DIR) != FS_FLAG_DIR) {													// Trying to read an directory entry using an file... why?
		return Null;
	} else if (entry == 0) {																				// Current directory?
		return StrDuplicate(".");
	} else if (entry == 1) {																				// Parent directory?
		return StrDuplicate("..");
	}
	
	PCHFsMountInfo info = (PCHFsMountInfo)dir->priv;
	
	if (info->dev->read == Null) {																			// We have the read function... right?
		return Null;
	} else if ((info->dev->flags & FS_FLAG_FILE) != FS_FLAG_FILE) {											// It's a file?
		return Null;
	}
	
	PCHFsINode inode = (PCHFsINode)MemAllocate(512);														// Alloc space for reading the inode
	
	if (inode == Null) {
		return Null;																						// Failed...
	}
	
	if (!FsReadFile(info->dev, dir->inode * 512, 512, (PUInt8)inode)) {										// Read the inode
		MemFree((UIntPtr)inode);
		return Null;
	}
	
	UIntPtr lba = inode->next_block;																		// Get the next lba
	
	if (lba == 0) {
		MemFree((UIntPtr)inode);
		return Null;
	}
	
	PUInt8 buff = (PUInt8)MemAllocate(512);																	// Alloc some memory for reading the disk
	PCHFsINode ent = inode;
	
	if (buff == Null) {
		MemFree((UIntPtr)inode);																			// ...
		return Null;
	}
	
	for (UIntPtr i = 0;; i++) {																				// Let's search for the entry!
		if (ent->type == 0x00) {																			// End of the directory?
			MemFree((UIntPtr)buff);																			// Yes :(
			MemFree((UIntPtr)inode);
			return Null;
		} else if (i == entry - 2) {
			UIntPtr nlen = ent->name_length;																// Save the name length
			PChar ret = (PChar)MemAllocate(nlen + 1);														// Alloc space for it
			
			if (ret == Null) {
				MemFree((UIntPtr)buff);																		// :(
				return Null;
			}
			
			StrCopyMemory(ret, ent->name, nlen);															// Copy it
			MemFree((UIntPtr)buff);																			// Free the buffer
			
			ret[nlen] = '\0';																				// End the return string with a NUL character
			
			return ret;
		}
		
		if (lba == 0) {																						// End of the chain?
			MemFree((UIntPtr)buff);																			// Yes :(
			MemFree((UIntPtr)inode);
			return Null;
		} else if (!FsReadFile(info->dev, lba * 512, 512, buff)) {											// Read the sector
			MemFree((UIntPtr)buff);																			// Failed...
			MemFree((UIntPtr)inode);
			return Null;
		}
		
		ent = (PCHFsINode)buff;																				// As we start with the first entry of the directory, we need to point the ent to the buffer now
		lba = ent->next_block;																				// Follow to the next block
	}
	
	MemFree((UIntPtr)buff);
	MemFree((UIntPtr)inode);
	
	return Null;
}

PFsNode CHFsFindInDirectory(PFsNode dir, PChar name) {
	if ((dir == Null) || (name == Null) || (dir->priv == Null) || (dir->inode == 0)) {						// Let's do some null pointer checks first!
		return Null;
	} else if ((dir->flags & FS_FLAG_DIR) != FS_FLAG_DIR) {													// Trying to read an directory entry using an file... why?
		return Null;
	} else if (StrCompare(name, ".") || StrCompare(name, "..")) {											// The . and the .. entries doesn't really exists
		return Null;
	}
	
	PCHFsMountInfo info = (PCHFsMountInfo)dir->priv;
	
	if (info->dev->read == Null) {																			// We have the read function... right?
		return Null;
	} else if ((info->dev->flags & FS_FLAG_FILE) != FS_FLAG_FILE) {											// It's a file?
		return Null;
	}
	
	PCHFsINode inode = (PCHFsINode)MemAllocate(512);														// Alloc space for reading the inode
	
	if (inode == Null) {
		return Null;																						// Failed...
	}
	
	if (!FsReadFile(info->dev, dir->inode * 512, 512, (PUInt8)inode)) {										// Read the inode
		MemFree((UIntPtr)inode);
		return Null;
	}
	
	UIntPtr lba = inode->next_block;																		// Get the next lba
	UIntPtr olba = dir->inode;
	
	if (lba == 0) {
		MemFree((UIntPtr)inode);
		return Null;
	}
	
	PUInt8 buff = (PUInt8)MemAllocate(512);																	// Alloc some memory for reading the disk
	PCHFsINode ent = inode;
	
	if (buff == Null) {
		MemFree((UIntPtr)inode);																			// ...
		return Null;
	}
	
	while (1) {																								// Let's search for the entry!
		if (ent->type == 0x00) {																			// End of the directory?
			MemFree((UIntPtr)buff);																			// Yes :(
			MemFree((UIntPtr)inode);
			return Null;
		}
		
		PChar entname = (PChar)MemAllocate(ent->name_length + 1);											// Alloc space for copying the name of this entry
		
		if (entname == Null) {
			MemFree((UIntPtr)buff);																			// Failed...
			return Null;
		}
		
		StrCopyMemory(entname, ent->name, ent->name_length);												// Copy the name
		entname[ent->name_length] = '\0';																	// End it with a NUL character
		
		if (StrGetLength(entname) == StrGetLength(name)) {													// Same length?
			if (StrCompare(entname, name)) {																// SAME NAME?
				PFsNode node = (PFsNode)MemAllocate(sizeof(FsNode));										// Try to alloc space for the file node itself
				
				if (node == Null) {
					MemFree((UIntPtr)entname);																// ...
					MemFree((UIntPtr)buff);
					MemFree((UIntPtr)inode);
					return Null;
				}
				
				node->name = entname;																		// Set the name
				node->priv = info;																			// The priv info
				
				if (ent->type == 0x01) {																	// File?
					node->flags = FS_FLAG_FILE;																// Yes, set the file flag and the read function (for now)
					node->read = CHFsReadFile;
					node->write = Null;
					node->readdir = Null;
					node->finddir = Null;
				} else {
					node->flags = FS_FLAG_DIR;																// No, set the dir flag, the readdir entry and the finddir entry
					node->read = Null;
					node->write = Null;
					node->readdir = CHFsReadDirectoryEntry;
					node->finddir = CHFsFindInDirectory;
				}
				
				node->inode = olba;																			// Set the inode
				node->length = ent->data_length;															// The length
				node->open = CHFsOpenFile;																	// The open and close function
				node->close = CHFsCloseFile;
				
				MemFree((UIntPtr)buff);																		// Free the buffer
				MemFree((UIntPtr)inode);																	// Free the inode
				
				return node;																				// And return :)
			}
		}
		
		
		MemFree((UIntPtr)entname);																			// Free the copied name
		
		if (lba == 0) {																						// End of the chain?
			MemFree((UIntPtr)buff);																			// Yes :(
			MemFree((UIntPtr)inode);
			return Null;
		} else if (!FsReadFile(info->dev, lba * 512, 512, buff)) {											// Read the sector
			MemFree((UIntPtr)buff);																			// Failed...
			MemFree((UIntPtr)inode);
			return Null;
		}
		
		ent = (PCHFsINode)buff;																				// As we start with the first entry of the directory, we need to point the ent to the buffer now
		olba = lba;																							// Save the old lba
		lba = ent->next_block;																				// Follow to the next block
	}
	
	return Null;
}

Boolean CHFsProbe(PFsNode file) {
	if (file == Null) {																						// Sanity checks :)
		return False;
	} else if ((file->flags & FS_FLAG_FILE) != FS_FLAG_FILE) {
		return False;
	} else if (file->read == Null) {
		return False;
	}
	
	PCHFsHeader hdr = (PCHFsHeader)MemAllocate(512);														// Alloc space for reading the header
	
	if (hdr == Null) {
		return False;
	}
	
	if (!file->read(file, 0, 512, (PUInt8)hdr)) {															// Read it!
		MemFree((UIntPtr)hdr);
		return False;
	} else if (StrCompareMemory(hdr->magic, "CHFS", 4)) {													// Check the magic number
		MemFree((UIntPtr)hdr);																				// :)
		return True;
	}
	
	MemFree((UIntPtr)hdr);																					// Free the header
	
	return False;
}

PFsMountPoint CHFsMount(PFsNode file, PChar path) {
	if ((file == Null) || (path == Null)) {																	// Sanity checks :)
		return Null;
	} else if ((file->flags & FS_FLAG_FILE) != FS_FLAG_FILE) {
		return Null;
	} else if (file->read == Null) {
		return Null;
	}
	
	PCHFsHeader hdr = (PCHFsHeader)MemAllocate(512);														// Alloc space for reading the superblock/header
	
	if (hdr == Null) {
		return Null;
	}
	
	if (!file->read(file, 0, 512, (PUInt8)hdr)) {															// Read it!
		MemFree((UIntPtr)hdr);
		return Null;
	} else if (!StrCompareMemory(hdr->magic, "CHFS", 4)) {													// And check the magic number/signature
		MemFree((UIntPtr)hdr);
		return Null;
	}
	
	PCHFsMountInfo info = (PCHFsMountInfo)MemAllocate(sizeof(CHFsMountInfo));								// Alloc space for the mount info
	
	if (info == Null) {
		MemFree((UIntPtr)hdr);
		return Null;
	}
	
	info->dev = file;																						// Set the "dev" file
	
	StrCopyMemory(&info->hdr, hdr, sizeof(CHFsHeader));														// Copy the superblock/header
	MemFree((UIntPtr)hdr);																					// Free the buffer
	
	PFsMountPoint mp = (PFsMountPoint)MemAllocate(sizeof(FsMountPoint));									// Let's alloc space for the mount point struct
	
	if (mp == Null) {
		MemFree((UIntPtr)info);
		return Null;
	}
	
	mp->path = StrDuplicate(path);																			// Let's try to duplicate the path string
	
	if (mp->path == Null) {
		MemFree((UIntPtr)mp);
		MemFree((UIntPtr)info);
		return Null;
	}
	
	mp->type = StrDuplicate("CHFs");																		// And the type string
	
	if (mp->type == Null) {
		MemFree((UIntPtr)mp->path);
		MemFree((UIntPtr)mp);
		MemFree((UIntPtr)info);
		return Null;
	}
	
	mp->root = (PFsNode)MemAllocate(sizeof(FsNode));														// Create the root directory node
	
	if (mp->root == Null) {
		MemFree((UIntPtr)mp->type);
		MemFree((UIntPtr)mp->path);
		MemFree((UIntPtr)mp);
		MemFree((UIntPtr)info);
		return Null;
	}
	
	mp->root->name = StrDuplicate("\\");																	// Duplicate the name
	
	if (mp->root->name == Null) {
		MemFree((UIntPtr)mp->root);
		MemFree((UIntPtr)mp->type);
		MemFree((UIntPtr)mp->path);
		MemFree((UIntPtr)mp);
		MemFree((UIntPtr)info);
		return Null;
	}
	
	mp->root->priv = info;																					// Finally, fill everything!
	mp->root->flags = FS_FLAG_DIR;
	mp->root->inode = info->hdr.root_directory_start;
	mp->root->length = 0;
	mp->root->read = CHFsReadFile;
	mp->root->write = Null;
	mp->root->open = CHFsOpenFile;
	mp->root->close = CHFsCloseFile;
	mp->root->readdir = CHFsReadDirectoryEntry;
	mp->root->finddir = CHFsFindInDirectory;
	
	return mp;
}

Boolean CHFsUmount(PFsMountPoint mp) {
	if (mp == Null) {																						// Sanity checks
		return False;
	} else if (mp->root == Null) {
		return False;
	} else if (mp->root->priv == Null) {
		return False;
	} else if (!StrCompare(mp->type, "CHFs")) {
		return False;
	}
	
	PCHFsMountInfo info = (PCHFsMountInfo)mp->root->priv;
	
	FsCloseFile(info->dev);																					// Close the dev file
	MemFree((UIntPtr)info);																					// Free the info struct
	
	return True;
}

Void CHFsInit(Void) {
	PChar name = StrDuplicate("CHFs");																		// Let's get our type string
	
	if (name == Null) {
		return;																								// Failed...
	}
	
	FsAddType(name, CHFsProbe, CHFsMount, CHFsUmount);														// And add ourself into the type list
}