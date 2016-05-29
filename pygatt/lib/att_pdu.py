import att
import uuid

def __unpack_handle(pdu, ofs):
	return pdu[ofs] + (pdu[ofs+1] << 8)

def __pack_handle(handle):
	assert handle <= 0xFFFF
	assert handle >= 0
	return bytearray([handle & 0xFF, (handle >> 8) & 0xFF])

def new_error_resp(op, handle, ecode):
	assert type(op) is int
	assert type(handle) is int
	assert type(ecode) is int

	err = bytearray(5)
	err[0] = att.OP_ERROR
	err[1] = op
	err[2] = handle & 0xFF
	err[3] = (handle >> 8) & 0xFF
	err[4] = ecode
	return err

def new_read_by_group_req(startHandle, endHandle, uuid):
	pdu = bytearray([att.OP_READ_BY_GROUP_REQ])
	pdu += __pack_handle(startHandle)
	pdu += __pack_handle(endHandle)
	pdu += uuid.raw()[::-1]
	return pdu

def new_read_by_type_req(startHandle, endHandle, uuid):
	pdu = bytearray([att.OP_READ_BY_TYPE_REQ])
	pdu += __pack_handle(startHandle)
	pdu += __pack_handle(endHandle)
	pdu += uuid.raw()[::-1]
	return pdu

def new_find_info_req(startHandle, endHandle):
	pdu = bytearray([att.OP_FIND_INFO_REQ])
	pdu += __pack_handle(startHandle)
	pdu += __pack_handle(endHandle)
	return pdu

def parse_read_req(pdu):
	assert type(pdu) is bytearray
	return pdu[0], __unpack_handle(pdu, 1)

def parse_write(pdu):
	return pdu[0], __unpack_handle(pdu, 1), pdu[3:]

def parse_find_info_req(pdu):
	return pdu[0], __unpack_handle(pdu, 1), __unpack_handle(pdu, 3)

def parse_find_by_type_req(pdu):
	return pdu[0], __unpack_handle(pdu, 1), __unpack_handle(pdu, 3), \
		uuid.UUID(pdu[5:7], reverse=True), pdu[7:]

def parse_read_by_type_req(pdu):
	return pdu[0], __unpack_handle(pdu, 1), __unpack_handle(pdu, 3), \
		uuid.UUID(pdu[5:], reverse=True)

def parse_read_by_group_req(pdu):
	return pdu[0], __unpack_handle(pdu, 1), __unpack_handle(pdu, 3), \
		uuid.UUID(pdu[5:], reverse=True)

def parse_notify_indicate(pdu):
	return pdu[0], __unpack_handle(pdu, 1), pdu[3:]