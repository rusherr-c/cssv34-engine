/*
 *
 * Copyright (c) 2026 RuSHeRR
 *
 * Purpose: communicates with source engine servers
 *	using valve's protocol
 *
 * VDC: https://developer.valvesoftware.com/wiki/Server_queries
 *
*/
#ifndef _SERVERCOMMUNICATION_H
#define _SERVERCOMMUNICATION_H

#include "socket.h"
#include "utlmap.h"

#pragma pack(push, 1)
// Public server query structure
struct scquery_t
{
	netadr_t addr;		// send/recv address
	uint	type;		// EServerQuery
	bf_read msg;		// easy bitbuf access

	// response target
	IServerQueryResponse* response;

	double	sendTime;	// send time
	int		ping;		// response timeout (in ms)

	// how many retries we have done
	byte	retries;
};
#pragma pack(pop)

/*
 * Main server communication class
*/
class CServerCommunication : public CMsgHandler
{
public:
	CServerCommunication();
	CServerCommunication(CSocket* pSocket);
	~CServerCommunication();

	// Handles a frame of networking
	void RunFrame();

	// Query server info at specified address and return handle to query
	int QueryServerInfo(const netadr_t& adr, IServerQueryResponse* response);

	// Query information about players on this server and return handle to query
	int QueryPlayerDetails(const netadr_t& adr, IServerQueryResponse* response);

	// Query server rules and return handle to query
	int QueryServerRules(const netadr_t& adr, IServerQueryResponse* response);

	// Get Query at specified handle
	scquery_t& GetQuery(int handle);

public:
	// CMsgHandler
	virtual bool Process(const netadr_t& from, bf_read& msg);	

protected:
	// Process server info
	bool ProcessServerInfo(scquery_t& query);

	// Process information about players
	bool ProcessPlayerDetails(scquery_t& query);
	
	// Process server rules
	bool ProcessServerRules(scquery_t& query);

private:
	// Initializes query
	scquery_t InitQuery(const netadr_t& adr, uint type, IServerQueryResponse* response);

	// Update challenge for a single query
	void UpdateChallenge(int id, int challenge);

	// Retry server query request
	void RetryRequest(int id);

	// Run timeout checks, and other things
	void QueryFrame();

	CSocket* m_pSocket;	// Query socket for game server info

	// holds the list of all the currently active queries
	CUtlRBTree<scquery_t, unsigned short> m_Queries;

	CUtlMap<int, int> m_ChallengeList;
};

#endif // _SERVERCOMMUNICATION_H