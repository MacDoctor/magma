
/**
 * @file /magma/servers/smtp/relay.c
 *
 * @brief	Functions to relay messages via SMTP to the outbound mail server.
 */

#include "magma.h"

/**
 * @brief	Issue an smtp client QUIT command.
 * @param	client	 a pointer to the smtp client session to be closed.
 * @return	This function returns no value.
 */
void smtp_client_close(client_t *client) {

	if (client_write(client, PLACER("QUIT\r\n", 6)) >= 0) {
		client_read_line(client);
	}

	client_close(client);

	return;
}

/**
 * @brief	Connect to a randomly selected mail relay server, and wait for a successful banner message.
 * @param	premium		if set, a premium relay will be selected instead of a standard one.
 * @return	NULL on failure or a pointer to the newly established network client object connected to a mail relay on success.
 */
client_t * smtp_client_connect(int_t premium) {

	uint32_t num = 0;
	client_t *client;
	relay_t *relay = NULL;

	// If the premium flag is set, pick a random premium relay.
	if (premium && magma.relay.count.premium) {
		num = (rand_get_uint32() % magma.relay.count.premium);

		for (uint32_t i = 0; !relay && i < MAGMA_RELAY_INSTANCES; i++) {

			if (magma.relay.host[i] && magma.relay.host[i]->premium && !num) {
					relay = magma.relay.host[i];
			}
			else if (magma.relay.host[i] && magma.relay.host[i]->premium) {
				num--;
			}

		}

	}

	// Otherwise, if no premium relays are defined, or premium is zero, pick a standard relay.
	else {
		num = (rand_get_uint32() % magma.relay.count.standard);

		for (uint32_t i = 0; !relay && i < MAGMA_RELAY_INSTANCES; i++) {

			if (magma.relay.host[i] && !magma.relay.host[i]->premium && !num) {
					relay = magma.relay.host[i];
			}
			else if (magma.relay.host[i] && !magma.relay.host[i]->premium) {
				num--;
			}

		}

	}

	if (!relay) {
		log_pedantic("Unable to find a suitable mail relay to connect to.");
		return NULL;
	}

	// Connect
	if (!(client = client_connect(relay->name, relay->port))) {
		log_pedantic("Unable to establish a network connection with the mail relay. {host = %s:%u}", relay->name, relay->port);
		return NULL;
	}

	// If a valid timeout was provided.
	if (!magma.relay.timeout) {
		net_set_timeout(client->sockd, magma.relay.timeout, magma.relay.timeout);
	}

	// If SSL was indicated.
	if (relay->secure && client_secure(client)) {
		log_pedantic("Unable to establish a secure (aka encrypted) network connection with the mail relay. {host = %s:%u}", relay->name, relay->port);
		client_close(client);
		return NULL;
	}

	// Read the SMTP banner.
	if(client_read_line(client) <= 0 || *(st_char_get(client->buffer)) != '2') {
		log_pedantic("The mail relay did not provide a proper greeting. {host = %s:%u / response = %.*s}", relay->name, relay->port,
			st_length_get(&(client->line)) > 32 ? 32 : st_length_int(&(client->line)), st_char_get(&(client->line)));
		client_close(client);
		return NULL;
	}

	return client;
}

/**
 * @brief	Issue a EHLO command to an smtp server, or fall back to HELO, and wait for a successful response.
 * @param	client	a pointer to the network client to issue the remote command.
 * @return	-1 on failure or 1 on success.
 */
int_t smtp_client_send_helo(client_t *client) {

	int_t state;

	client_print(client, "EHLO %s\r\n", magma.host.name);

	if ((client_read_line(client)) <= 0) {
		log_pedantic("An error occurred while trying to read the hello response.");
		return -1;
	}
	// This server doesn't appear to support ESMTP
	else if (*(st_char_get(client->buffer)) != '2') {
		client_print(client, "HELO %s\r\n", magma.host.name);
		state = client_read_line(client);

		if (state <= 0 || *(st_char_get(client->buffer)) != '2') {
			log_pedantic("Tried sending the old fashioned HELO command and got back an error.");
			return -1;
		}

	}
	// It supports ESMTP, so read in the lines describing the server.
	else {
		state = 1;

		do {

			if (st_length_get(&(client->line)) < 4 || *(st_char_get(client->buffer) + 3) == ' ') {
				state = 0;
			}

		} while (state == 1 && client_read_line(client) > 0);

		if (state != 0) {
			log_pedantic("An error occurred while trying to find the end of the EHLO response.");
			return -1;
		}

	}

	return 1;
}

/**
 * @brief	Issue a MAIL FROM command to an smtp server, and wait for a successful response.
 * @param	client		a pointer to the network client to issue the MAIL FROM command.
 * @param	mailfrom	a pointer to a managed string containing the address parameter for the MAIL FROM command.
 * @param	send_size	if greater than 0, specify the optional SIZE parameter to the MAIL FROM command.
 * @return	-2 if the remote server rejected the command, -1 on general network failure, or 1 on success.
 */
int_t smtp_client_send_mailfrom(client_t *client, stringer_t *mailfrom, size_t send_size) {

	if (!send_size) {
		client_print(client, "MAIL FROM: <%.*s>\r\n", st_length_get(mailfrom), st_char_get(mailfrom));
	} else {
		client_print(client, "MAIL FROM: <%.*s> SIZE=%u\r\n", st_length_get(mailfrom), st_char_get(mailfrom), send_size);
	}

	if (client_read_line(client) <= 0) {
		log_pedantic("An error occurred while attempting to send the MAIL FROM command.");
		return -1;
	}
	else if (*(st_char_get(client->buffer)) != '2') {
		log_pedantic("An error occurred while attempting to send the MAIL FROM command. {mailfrom = %.*s / response = %.*s}",
			st_length_int(mailfrom), st_char_get(mailfrom), st_length_int(&(client->line)), st_char_get(&(client->line)));
		return -2;
	}

	return 1;
}

/**
 * @brief	Issue a MAIL FROM command to an smtp server with a null sender, and wait for a successful response.
 * @note	Null senders are used when the sender is not concerned about being notified about bounced messages.
 * @param	client		a pointer to the network client to issue the MAIL FROM command.
 * @return	-2 if the remote server rejected the command, -1 on general network failure, or 1 on success.
 */
int_t smtp_client_send_nullfrom(client_t *client) {

	client_print(client, "MAIL FROM: <>\r\n");

	if (client_read_line(client) <= 0) {
		log_pedantic("An error occurred while attempting to send the MAIL FROM command.");
		return -1;
	}
	else if (*(st_char_get(client->buffer)) != '2') {
		log_pedantic("An error occurred while attempting to send the MAIL FROM command. {mailfrom = NULL / response = %.*s}",
			st_length_int(&(client->line)), st_char_get(&(client->line)));
		return -2;
	}

	return 1;
}

/**
 * @brief	Issue a RCPT TO command to an smtp server, and wait for a successful response.
 * @param	client		a pointer to the network client to issue the RCPT TO command.
 * @param	rcptto		a pointer to a managed string containing the recipient address parameter for the RCPT TO command.
 * @return	-2 if the remote server rejected the command, -1 on general network failure, or 1 on success.
 */
int_t smtp_client_send_rcptto(client_t *client, stringer_t *rcptto) {

	client_print(client, "RCPT TO: <%.*s>\r\n", st_length_get(rcptto), st_char_get(rcptto));

	if (client_read_line(client) <= 0) {
		log_pedantic("An error occurred while attempting to send the RCPT TO command.");
		return -1;
	}
	else if (*(st_char_get(client->buffer)) != '2') {
		log_pedantic("An error occurred while attempting to send the RCPT TO command. {rcptto = %.*s / response = %.*s}",
			st_length_int(rcptto), st_char_get(rcptto), st_length_int(&(client->line)), st_char_get(&(client->line)));
		return -2;
	}

	return 1;
}

/**
 * @brief	Issue a DATA command to an smtp server, and wait for a successful response.
 * @param	client		a pointer to the network client to issue the DATA command.
 * @param	message		a pointer to a managed string containing the body of the message to be sent.
 * @return	-2 if the remote server rejected the command, -1 on general network failure, or 1 on success.
 */
int_t smtp_client_send_data(client_t *client, stringer_t *message) {

	int64_t sent = 0, line = 0;

	// Send the DATA command and confirm the proceed response was recieved in response.
	if ((sent = client_write(client, PLACER("DATA\r\n", 6))) != 6 || (line = client_read_line(client)) <= 0 || !pl_starts_with_char(client->line, '3')) {
		log_pedantic("A%serror occurred while trying to send the DATA command.%s", (sent != 6 || line <= 0 ? " network " : "n "),
			(sent == 6 && line > 0 ? st_char_get(st_quick(MANAGEDBUF(1024), " { response = %.*s }", st_length_int(&(client->line)),
			st_char_get(&(client->line)))) : ""));
		return (sent != 6 || line <= 0 ? -1 : -2);
	}

	// Dot stuff the message using the string replace function, which requires a message be stored in a jointed
	// buffer, that can be dynamially reallocated if necessary.
	st_replace(&(message), PLACER("\n.", 2), PLACER("\n..", 3));

	// Send the message and confirm all of the bytes were sent.
	if ((sent = client_write(client, message)) != st_length_get(message)) {
		log_pedantic("Message relay failed. { sent = %li / size = %zu }", sent, st_length_get(message));
	}

	// If the message doesn't end witha line break, we'll send a line terminator here, so the termination sequence sent below
	// will appear on a line by itself.
	if (!st_cmp_cs_ends(message, PLACER("\n", 1))) {
		client_write(client, PLACER("\r\n", 2));
	}

	// Read in the result code to see if the message was relayed successfully. If it fails, print the resulting response message.
	if ((sent = client_write(client, PLACER(".\r\n", 3))) != 3 || (line = client_read_line(client)) <= 0 || !pl_starts_with_char(client->line, '2')) {
		log_pedantic("A%serror occurred while attempting to transmit the message.%s", (sent != 3 || line <= 0 ? " network " : "n "),
			(sent == 6 && line > 0 ? st_char_get(st_quick(MANAGEDBUF(1024), " { response = %.*s }", st_length_int(&(client->line)),
			st_char_get(&(client->line)))) : ""));
		return (sent != 3 || line <= 0 ? -1 : -2);
	}

	return 1;
}
