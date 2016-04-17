/*
 * att.h
 *
 *  Created on: Mar 24, 2016
 *      Author: james
 */

#ifndef BLE_ATT_H_
#define BLE_ATT_H_

/* Len of signature in write signed packet */
#define ATT_SIGNATURE_LEN		12

/* Attribute Protocol Opcodes */
#define ATT_OP_ERROR			0x01
#define ATT_OP_MTU_REQ			0x02
#define ATT_OP_MTU_RESP			0x03
#define ATT_OP_FIND_INFO_REQ		0x04
#define ATT_OP_FIND_INFO_RESP		0x05
#define ATT_OP_FIND_BY_TYPE_REQ		0x06
#define ATT_OP_FIND_BY_TYPE_RESP	0x07
#define ATT_OP_READ_BY_TYPE_REQ		0x08
#define ATT_OP_READ_BY_TYPE_RESP	0x09
#define ATT_OP_READ_REQ			0x0A
#define ATT_OP_READ_RESP		0x0B
#define ATT_OP_READ_BLOB_REQ		0x0C
#define ATT_OP_READ_BLOB_RESP		0x0D
#define ATT_OP_READ_MULTI_REQ		0x0E
#define ATT_OP_READ_MULTI_RESP		0x0F
#define ATT_OP_READ_BY_GROUP_REQ	0x10
#define ATT_OP_READ_BY_GROUP_RESP	0x11
#define ATT_OP_WRITE_REQ		0x12
#define ATT_OP_WRITE_RESP		0x13
#define ATT_OP_WRITE_CMD		0x52
#define ATT_OP_PREP_WRITE_REQ		0x16
#define ATT_OP_PREP_WRITE_RESP		0x17
#define ATT_OP_EXEC_WRITE_REQ		0x18
#define ATT_OP_EXEC_WRITE_RESP		0x19
#define ATT_OP_HANDLE_NOTIFY		0x1B
#define ATT_OP_HANDLE_IND		0x1D
#define ATT_OP_HANDLE_CNF		0x1E
#define ATT_OP_SIGNED_WRITE_CMD		0xD2

/* Error codes for Error response PDU */
#define ATT_ECODE_INVALID_HANDLE		0x01
#define ATT_ECODE_READ_NOT_PERM			0x02
#define ATT_ECODE_WRITE_NOT_PERM		0x03
#define ATT_ECODE_INVALID_PDU			0x04
#define ATT_ECODE_AUTHENTICATION		0x05
#define ATT_ECODE_REQ_NOT_SUPP			0x06
#define ATT_ECODE_INVALID_OFFSET		0x07
#define ATT_ECODE_AUTHORIZATION			0x08
#define ATT_ECODE_PREP_QUEUE_FULL		0x09
#define ATT_ECODE_ATTR_NOT_FOUND		0x0A
#define ATT_ECODE_ATTR_NOT_LONG			0x0B
#define ATT_ECODE_INSUFF_ENCR_KEY_SIZE		0x0C
#define ATT_ECODE_INVAL_ATTR_VALUE_LEN		0x0D
#define ATT_ECODE_UNLIKELY			0x0E
#define ATT_ECODE_INSUFF_ENC			0x0F
#define ATT_ECODE_UNSUPP_GRP_TYPE		0x10
#define ATT_ECODE_INSUFF_RESOURCES		0x11
/* Application error */
#define ATT_ECODE_IO				0x80
#define ATT_ECODE_TIMEOUT			0x81
#define ATT_ECODE_ABORTED			0x82

#define ATT_MAX_VALUE_LEN			512
#define ATT_DEFAULT_L2CAP_MTU			48
#define ATT_DEFAULT_LE_MTU			23

#define ATT_CID					4
#define ATT_PSM					31

/* Flags for Execute Write Request Operation */
#define ATT_CANCEL_ALL_PREP_WRITES		0x00
#define ATT_WRITE_ALL_PREP_WRITES		0x01

/* Find Information Response Formats */
#define ATT_FIND_INFO_RESP_FMT_16BIT		0x01
#define ATT_FIND_INFO_RESP_FMT_128BIT		0x02

#endif /* BLE_ATT_H_ */
