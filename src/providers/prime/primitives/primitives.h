
/**
 * @file /magma/src/providers/prime/primitives/primitives.h
 *
 * @brief DESCRIPTIONxxxGOESxxxHERE
 *
 * $Author$
 * $Date$
 * $Revision$
 *
 */

#ifndef PRIME_PRIMITIVES_H
#define PRIME_PRIMITIVES_H

#define PRIME_MAX_1_BYTE 255
#define PRIME_MAX_2_BYTE 65535
#define PRIME_MAX_3_BYTE 16777215
#define PRIME_MAX_4_BYTE 4294967295

/// Object data types.
typedef uint32_t prime_size_t;

/// Field data types.
typedef uint8_t prime_field_type_t;
typedef placer_t prime_field_data_t;

/***
 *	@brief PRIME fields inside a packed object.
 *	@note The fields are stored inside an array, but the data variable is a place holder
 *			which points into the packed stream of bytes.
 */
typedef struct __attribute__ ((packed)) {
	prime_field_type_t type;	//!< The field type identifier. (1b)
	prime_field_data_t payload; //!< The field payload represented by a placeholder.
} prime_field_t;

/***
 *	@brief The interface for PRIME objects.
 */
typedef struct __attribute__ ((packed)) {

	//!< The object header.
	prime_type_t type;			//!< The object type identifier. (2b)
	prime_size_t size;			//!< The size of the object data (not including the header). (3b or 4b)

	//!< The array of parsed fields inside the packed object buffer.
	prime_size_t count;
	prime_field_t fields[];

} prime_object_t;

/***
 *	@brief Used to read through a binary stream containg a PRIME object.
 */
typedef struct __attribute__ ((packed)) {
	void *cursor;
	size_t remaining;
	placer_t buffer;
} prime_reader_t;

/// reader.c
int_t   prime_reader_open(stringer_t *data, prime_reader_t *reader);
int_t   prime_reader_payload(prime_reader_t *reader, int_t bytes, placer_t payload);
int_t   prime_reader_size(prime_reader_t *reader, int_t bytes);
int_t   prime_reader_type(prime_reader_t *reader);

/// objects.c
size_t   prime_object_size_max(prime_type_t type);
size_t   prime_object_size_min(prime_type_t type);
chr_t *  prime_object_type(prime_type_t type);

/// headers.c
stringer_t *  prime_header_encrypted_message_write(size_t size, stringer_t *output);
stringer_t *  prime_header_encrypted_org_key_write(size_t size, stringer_t *output);
stringer_t *  prime_header_encrypted_user_key_write(size_t size, stringer_t *output);
size_t        prime_header_length(prime_type_t type);
stringer_t *  prime_header_org_key_write(size_t size, stringer_t *output);
stringer_t *  prime_header_org_signet_write(size_t size, stringer_t *output);
int_t         prime_header_read(stringer_t *object, uint16_t *type, uint32_t *size);
stringer_t *  prime_header_user_key_write(size_t size, stringer_t *output);
stringer_t *  prime_header_user_signet_write(size_t size, stringer_t *output);
stringer_t *  prime_header_user_signing_request_write(size_t size, stringer_t *output);
stringer_t *  prime_header_write(prime_type_t type, size_t size, stringer_t *output);

/// fields.c
int_t         prime_field_size_length(prime_field_type_t field);
size_t        prime_field_size_max(prime_type_t type, prime_field_type_t field);
stringer_t *  prime_field_write(prime_type_t type, prime_field_type_t field, size_t size, stringer_t *data, stringer_t *output);

#endif

