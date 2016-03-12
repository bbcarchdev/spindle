/* Spindle: Co-reference aggregation engine
 *
 * Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2016 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define MQ_CONNECTION_STRUCT_DEFINED   1
#define MQ_MESSAGE_STRUCT_DEFINED      1
#define MQ_ERRBUF_LEN                  128

#include "p_spindle-generate.h"

/* Message Queue (libmq) implementation which uses the Spindle relational
 * database as a data source
 */

struct mq_connection_struct
{
	MQCONNIMPL *impl;
	MQ_CONNECTION_COMMON_MEMBERS;
	SQL *sql;
};

struct mq_message_struct
{
	MQMESSAGEIMPL *impl;
	MQ_MESSAGE_COMMON_MEMBERS;
	char *buf;
};

static int spindle_mq_register_(const char *scheme, void *handle);
static MQ *spindle_mq_construct_(const char *uri, const char *reserved1, const char *reserved2);
static MQMESSAGE *spindle_mqmessage_construct_(MQ *self);

/* MQ implementation */
static unsigned long spindle_mq_release_(MQ *self);
static int spindle_mq_error_(MQ *self);
static const char *spindle_mq_errmsg_(MQ *self);
static MQSTATE spindle_mq_state_(MQ *self);
static int spindle_mq_connect_recv_(MQ *self);
static int spindle_mq_connect_send_(MQ *self);
static int spindle_mq_disconnect_(MQ *self);
static int spindle_mq_next_(MQ *self, MQMESSAGE **msg);
static int spindle_mq_deliver_(MQ *self);
static int spindle_mq_create_(MQ *self, MQMESSAGE **msg);
static int spindle_mq_set_cluster_(MQ *self, CLUSTER *cluster);
static CLUSTER *spindle_mq_cluster_(MQ *self);

/* MQMESSAGE implementation */
static unsigned long spindle_mqmessage_release_(MQMESSAGE *self);
static MQMSGKIND spindle_mqmessage_kind_(MQMESSAGE *self);
static int spindle_mqmessage_accept_(MQMESSAGE *self);
static int spindle_mqmessage_reject_(MQMESSAGE *self);
static int spindle_mqmessage_pass_(MQMESSAGE *self);
static int spindle_mqmessage_send_(MQMESSAGE *self);
static int spindle_mqmessage_set_type_(MQMESSAGE *self, const char *type);
static const char *spindle_mqmessage_type_(MQMESSAGE *self);
static int spindle_mqmessage_set_subject_(MQMESSAGE *self, const char *type);
static const char *spindle_mqmessage_subject_(MQMESSAGE *self);
static int spindle_mqmessage_set_address_(MQMESSAGE *self, const char *address);
static const char *spindle_mqmessage_address_(MQMESSAGE *self);
static const unsigned char *spindle_mqmessage_body_(MQMESSAGE *self);
static size_t spindle_mqmessage_len_(MQMESSAGE *self);
static int spindle_mqmessage_add_bytes_(MQMESSAGE *self, unsigned char *buf, size_t len);

static MQCONNIMPL spindle_mq_connection_impl_ = {
	NULL,
	NULL,
	spindle_mq_release_,
	spindle_mq_error_,
	spindle_mq_errmsg_,
	spindle_mq_state_,
	spindle_mq_connect_recv_,
	spindle_mq_connect_send_,
	spindle_mq_disconnect_,
	spindle_mq_next_,
	spindle_mq_deliver_,
	spindle_mq_create_,
	spindle_mq_set_cluster_,
	spindle_mq_cluster_
};

static MQMESSAGEIMPL spindle_mqmessage_impl_ = {
	NULL,
	NULL,
	spindle_mqmessage_release_,
	spindle_mqmessage_kind_,
	spindle_mqmessage_accept_,
	spindle_mqmessage_reject_,
	spindle_mqmessage_pass_,
	spindle_mqmessage_send_,
	spindle_mqmessage_set_type_,
	spindle_mqmessage_type_,
	spindle_mqmessage_set_subject_,
	spindle_mqmessage_subject_,
	spindle_mqmessage_set_address_,
	spindle_mqmessage_address_,
	spindle_mqmessage_body_,
	spindle_mqmessage_len_,
	spindle_mqmessage_add_bytes_
};

int
spindle_mq_init(void *handle)
{
	return sql_scheme_foreach(spindle_mq_register_, handle);
}

static int
spindle_mq_register_(const char *scheme, void *handle)
{
	char schemebuf[48];
	
	if(strlen(scheme) >= 32)
	{
		return 0;
	}
	strcpy(schemebuf, "spindle+");
	strcat(schemebuf, scheme);
	mq_register(schemebuf, spindle_mq_construct_, handle);
	return 0;
}

static MQ *
spindle_mq_construct_(const char *uri, const char *reserved1, const char *reserved2)
{
	MQ *mq;
	char *p;

	(void) reserved1;
	(void) reserved2;
	
	mq = (MQ *) calloc(1, sizeof(MQ));
	if(!mq)
	{
		return NULL;
	}
	p = strdup(uri);
	if(!p)
	{
		free(mq);
		return NULL;
	}
	mq->impl = &spindle_mq_connection_impl_;
	mq->uri = p;
	return mq;
}

static unsigned long
spindle_mq_release_(MQ *self)
{
	if(self->sql)
	{
		sql_disconnect(self->sql);
		self->sql = NULL;
	}
	free(self->errmsg);
	free(self->uri);
	free(self);
	return 0;
}

static int
spindle_mq_error_(MQ *self)
{
	if(self->errcode || self->syserr)
	{
		return 1;
	}
	return 0;
}

static const char *
spindle_mq_errmsg_(MQ *self)
{
	if(!self->errmsg)
	{
		self->errmsg = (char *) malloc(MQ_ERRBUF_LEN);
		if(!self->errmsg)
		{
			return "Memory allocation error obtaining error message";
		}
	}	
	self->errmsg[0] = 0;
	if(self->syserr)
	{
		strerror_r(self->syserr, self->errmsg, MQ_ERRBUF_LEN);
		return self->errmsg;
	}
	if(self->errcode)
	{
		snprintf(self->errmsg, MQ_ERRBUF_LEN, "Unknown error #%d", self->errcode);
		return self->errmsg;
	}
	return "Success";
}

static MQSTATE
spindle_mq_state_(MQ *self)
{
	RESET_ERROR(self);
	return self->state;
}

static int
spindle_mq_connect_recv_(MQ *self)
{
	RESET_ERROR(self);
	if(self->state != MQS_DISCONNECTED)
	{
		SET_SYSERR(self, EINVAL);
		return -1;
	}
	twine_logf(LOG_DEBUG, PLUGIN_NAME ": MQ: establishing connection to <%s>\n", self->uri + 8);
	self->sql = sql_connect(self->uri + 8);
	if(!self->sql)
	{
		SET_ERRNO(self);
		twine_logf(LOG_ERR, PLUGIN_NAME ": MQ: failed to connect to SQL database\n", self->uri + 8);
		return -1;
	}
	return 0;
}

static int spindle_mq_connect_send_(MQ *self)
{
	/* You can't send a message to this queue handler */
	SET_SYSERR(self, EPERM);
	return -1;
}

static int spindle_mq_disconnect_(MQ *self)
{
	RESET_ERROR(self);
	self->state = MQS_DISCONNECTED;
	if(self->sql)
	{
		sql_disconnect(self->sql);
		self->sql = NULL;
	}
	return 0;
}

static int
spindle_mq_next_(MQ *self, MQMESSAGE **msg)
{
	SQL_STATEMENT *rs;
	int nodeid, nodecount;
	MQMESSAGE *p;
	
	if(!self->sql)
	{
		SET_SYSERR(self, EINVAL);
		return -1;
	}
	*msg = NULL;
	if(self->cluster)
	{
		nodeid = cluster_index(self->cluster, 0);
		nodecount = cluster_total(self->cluster);
	}
	else
	{
		nodeid = 0;
		nodecount = 1;
	}
	while(1)
	{
		rs = sql_queryf(self->sql,
						"SELECT \"id\" "
						" FROM "
						" \"state\" "
						" WHERE "
						"\"state\".\"status\" = %Q AND "
						" \"state\".\"tinyhash\" %% %d = %d "
						" LIMIT 1",
						"DIRTY", nodecount, nodeid);
		if(!rs)
		{
			twine_logf(LOG_CRIT,  PLUGIN_NAME ": MQ: %s\n", sql_error(self->sql));
			return -1;
		}
		if(sql_stmt_eof(rs))
		{
			sql_stmt_destroy(rs);
			sleep(1);
			return 0;
		}
		p = spindle_mqmessage_construct_(self);
		if(!p)
		{
			SET_ERRNO(self);
			twine_logf(LOG_CRIT, PLUGIN_NAME ": MQ: failed to create new message\n");
			return -1;
		}
		twine_logf(LOG_DEBUG, PLUGIN_NAME ": MQ: next item is {%s}\n", sql_stmt_str(rs, 0));
		p->buf = strdup(sql_stmt_str(rs, 0));
		if(!p->buf)
		{
			SET_ERRNO(self);
			twine_logf(LOG_CRIT, PLUGIN_NAME ": MQ: failed to duplicate buffer for incoming message\n");
			free(p);
			return -1;
		}
		sql_stmt_destroy(rs);
		*msg = p;
		break;
	}
	return 0;
}

static int
spindle_mq_deliver_(MQ *self)
{
	/* This engine doesn't have outgoing messages */
	SET_SYSERR(self, EPERM);
	return -1;
}

static int
spindle_mq_create_(MQ *self, MQMESSAGE **msg)
{
	/* This engine can't create outgoing messages */
	(void) msg;

	SET_SYSERR(self, EINVAL);
	return -1;
}

/* Set the cluster associated with a connection */
static int
spindle_mq_set_cluster_(MQ *self, CLUSTER *cluster)
{
	self->cluster = cluster;
	return 0;
}

/* Obtain the cluster (if any) associated with a connection */
static CLUSTER *
spindle_mq_cluster_(MQ *self)
{
	return self->cluster;
}

/* (Internal) create a new MQ message object */
static MQMESSAGE *
spindle_mqmessage_construct_(MQ *self)
{
	MQMESSAGE *p;

	p = (MQMESSAGE *) calloc(1, sizeof(MQMESSAGE));
	if(!p)
	{
		SET_ERRNO(self);
		return NULL;
	}
	p->impl = &spindle_mqmessage_impl_;
	p->connection = self;
	return p;
}

/* Release (destroy) a message */
static unsigned long
spindle_mqmessage_release_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	free(self);
	return 0;
}

static MQMSGKIND
spindle_mqmessage_kind_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return self->kind;
}

/* Mark an incoming message as being accepted */
static int
spindle_mqmessage_accept_(MQMESSAGE *self)
{   
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	return 0;
}

static int
spindle_mqmessage_reject_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	return 0;
}

static int
spindle_mqmessage_pass_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	if(self->kind != MQK_INCOMING)
	{
		SET_SYSERR(self->connection, EINVAL);
		return -1;
	}
	return 0;
}

/* Set the content-type of an outgoing message */
static int
spindle_mqmessage_set_type_(MQMESSAGE *self, const char *type)
{
	(void) type;

	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* Retrieve the content-type of a message */
static const char *
spindle_mqmessage_type_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return SPINDLE_URI_MIME;
}

/* Set the subject of a message */
static int
spindle_mqmessage_set_subject_(MQMESSAGE *self, const char *subject)
{
	(void) subject;

	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* Retrieve the content-type of a message */
const char *
spindle_mqmessage_subject_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return self->buf;
}

/* Set the address (destination) of an outgoing message, replacing any
 * previously-set destination
 */
static int
spindle_mqmessage_set_address_(MQMESSAGE *self, const char *address)
{
	(void) address;

	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* Retrieve the address of a message: if it's an outging message, it's the
 * destination; if it's an incoming message, it's the source
 */
static const char *
spindle_mqmessage_address_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return "spindle:";
}

/* Retrieve the body of an incoming message */
static const unsigned char *
spindle_mqmessage_body_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return (const unsigned char *) self->buf;
}

/* Retrieve the length of an incoming message body, in bytes */
static size_t
spindle_mqmessage_len_(MQMESSAGE *self)
{
	RESET_ERROR(self->connection);
	return strlen(self->buf);
}

/* Add a sequence of bytes to an outgoing message body */
static int
spindle_mqmessage_add_bytes_(MQMESSAGE *self, unsigned char *buf, size_t len)
{
	(void) buf;
	(void) len;

	SET_SYSERR(self->connection, EPERM);
	return -1;
}

/* Send an outgoing message */
static int
spindle_mqmessage_send_(MQMESSAGE *self)
{
	SET_SYSERR(self->connection, EPERM);
	return -1;
}
