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

#include "pch_serverbrowser.h"

/*
 * Purpose: Comparison function used in query redblack tree
*/
bool QueryLessFunc(const scquery_t& item1, const scquery_t& item2)
{
	// compare port then ip
	if (item1.addr.port < item2.addr.port)
		return true;
	else if (item1.addr.port > item2.addr.port)
		return false;

	int ip1 = *(int*)&item1.addr.ip;
	int ip2 = *(int*)&item2.addr.ip;

	if (ip1 < ip2)
		return true;
	else if (ip1 > ip2)
		return false;

	// now compare types

	return item1.type < item2.type;
}

/*
 * Purpose: Constructor
*/
CServerCommunication::CServerCommunication() 
	: m_Queries(0, 255, QueryLessFunc), m_ChallengeList(0, 255) {
	
	SetDefLessFunc(m_ChallengeList);
	m_pSocket = new CSocket(4510);
	m_pSocket->AddHandler(this);
}

CServerCommunication::CServerCommunication(CSocket* pSocket)
	: m_Queries(0, 255, QueryLessFunc), m_ChallengeList(0, 255) {
	SetDefLessFunc(m_ChallengeList);
	m_pSocket = pSocket;
	m_pSocket->AddHandler(this);
}

/*
 * Purpose: Destructor
*/
CServerCommunication::~CServerCommunication() {
	delete m_pSocket;
}

/*
 * Purpose: Handles a frame of networking
*/
void CServerCommunication::RunFrame() {
	if (m_pSocket)
		m_pSocket->Frame();

	QueryFrame();
}

/*
 * Purpose: Query server info at specified address and return handle to query
*/
int CServerCommunication::QueryServerInfo(const netadr_t& adr, IServerQueryResponse* response) {
	if (!adr.IsValid())
		return 0;

	char buffer[64];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_INFO_REQUEST);
	msg.WriteString(A2S_KEY_STRING);

	// Sendmessage
	m_pSocket->Send(adr, msg);
	scquery_t query = InitQuery(adr, k_ePingServer, response);

	int id = m_Queries.Insert(query);

	return id;
}

/*
 * Purpose: Query information about players on this server and return handle to query
*/
int CServerCommunication::QueryPlayerDetails(const netadr_t& adr, IServerQueryResponse* response) {
	if (!adr.IsValid())
		return 0;

	char buffer[64];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_PLAYER_REQUEST);
	msg.WriteLong(-1);

	// Sendmessage
	m_pSocket->Send(adr, msg);
	scquery_t query = InitQuery(adr, k_ePlayerDetails, response);

	int id = m_Queries.Insert(query);
	m_ChallengeList.Insert(id, -1);

	return id;
}

/*
 * Purpose: Query server rules and return handle to query
*/
int CServerCommunication::QueryServerRules(const netadr_t& adr, IServerQueryResponse* response) {
	if (!adr.IsValid())
		return 0;

	char buffer[64];
	bf_write msg(buffer, sizeof(buffer));

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(A2S_RULES_REQUEST);
	msg.WriteLong(-1);

	// Sendmessage
	m_pSocket->Send(adr, msg);
	scquery_t query = InitQuery(adr, k_eServerRules, response);

	int id = m_Queries.Insert(query);
	m_ChallengeList.Insert(id, -1);

	return id;
}

/*
 * Get Query at specified handle
*/
scquery_t& CServerCommunication::GetQuery(int handle) {
	if (m_Queries.IsValidIndex(handle))
	{
		return m_Queries[handle];
	}

	// return a dummy
	static scquery_t dummy;
	memset(&dummy, 0, sizeof(dummy));
	return dummy;
}

/*
 * Purpose: CMsgHandler
*/
bool CServerCommunication::Process(const netadr_t& from, bf_read& msg) {
	scquery_t search{};
	search.addr = from;

	byte c = msg.ReadByte();
	switch (c)
	{

	case S2A_INFO_REPLY:
	{
		// find our pingserver query
		search.type = k_ePingServer;
		int id = m_Queries.Find(search);
		if (!m_Queries.IsValidIndex(id))
			return false;

		m_Queries[id].msg = msg;
		m_Queries[id].ping = (int)((Plat_FloatTime() - m_Queries[id].sendTime) * 1000);

		bool ret = ProcessServerInfo(m_Queries[id]);
		if (!ret)
			Warning("Failed processing server info at query %i: addr %s\n",
				id, m_Queries[id].addr.ToString());

		m_Queries.RemoveAt(id);

		break;
	}
	case S2A_PLAYER_REPLY:
	{
		// find our players query
		search.type = k_ePlayerDetails;
		int id = m_Queries.Find(search);
		if (!m_Queries.IsValidIndex(id))
			return false;

		m_Queries[id].msg = msg;
		m_Queries[id].ping = (int)((Plat_FloatTime() - m_Queries[id].sendTime) * 1000);

		bool ret = ProcessPlayerDetails(m_Queries[id]);

		if (!ret)
			Warning("Failed processing player details at query %i: addr %s\n",
				id, m_Queries[id].addr.ToString());

		m_Queries.RemoveAt(id);

		break;
	}
	case S2A_RULES_REPLY:
	{
		// find our rules query
		search.type = k_eServerRules;
		int id = m_Queries.Find(search);
		if (!m_Queries.IsValidIndex(id))
			return false;

		m_Queries[id].msg = msg;
		m_Queries[id].ping = (int)((Plat_FloatTime() - m_Queries[id].sendTime) * 1000);

		bool ret = ProcessServerRules(m_Queries[id]);

		if (!ret)
			Warning("Failed processing server rules at query %i: addr %s\n",
				id, m_Queries[id].addr.ToString());

		m_Queries.RemoveAt(id);

		break;
	}
	case S2C_CHALLENGE:
	{
		int challenge = msg.ReadLong();

		// search for player request first
		search.type = k_ePlayerDetails;
		int id = m_Queries.Find(search);

		UpdateChallenge(id, challenge);
		if (m_Queries.IsValidIndex(id))
			RetryRequest(id);

		// next, try server rules
		search.type = k_eServerRules;
		id = m_Queries.Find(search);

		UpdateChallenge(id, challenge);
		if (m_Queries.IsValidIndex(id))
			RetryRequest(id);

		break;
	}

	default:
		break;
	}

	return true;
}

/*
 * Process server info
*/
bool CServerCommunication::ProcessServerInfo(scquery_t& query)
{
	bf_read& msg = query.msg;
	serveritem_t server{};
	server.m_NetAdr = query.addr;
	server.m_nProtocolVersion = msg.ReadByte();

	msg.ReadString(server.m_szServerName, sizeof(server.m_szServerName));
	msg.ReadString(server.m_szMap, sizeof(server.m_szMap));
	msg.ReadString(server.m_szGameDir, sizeof(server.m_szGameDir));

	msg.ReadString(server.m_szGameDescription, sizeof(server.m_szGameDescription));
	server.m_nAppID = msg.ReadShort();

	// player info
	server.m_nPlayers = msg.ReadByte();
	server.m_nMaxPlayers = msg.ReadByte();
	server.m_nBotPlayers = msg.ReadByte();

	// Password?
	msg.ReadByte(); // server type
	msg.ReadByte(); // env

	server.m_bPassword = msg.ReadByte();
	server.m_bSecure = msg.ReadByte();
	server.m_nPing = query.ping;
	msg.ReadString(server.m_szGameVersion, sizeof(server.m_szGameVersion));

	if (msg.IsOverflowed())
		return false;

	if (query.response)
		query.response->ServerResponded(server);

	return true;
}

/*
 * Process information about players
*/
bool CServerCommunication::ProcessPlayerDetails(scquery_t& query)
{
	if (!query.response)
		return false;

	bf_read& msg = query.msg;
	int numPlayers = msg.ReadByte();
	if (!numPlayers) {
		query.response->PlayersFailedToRespond();
		return false;
	}

	char name[64];
	int score = 0;
	float duration = 0;
	for (int i = 0; i < numPlayers; i++)
	{
		if (i != msg.ReadByte());
			/* i don't know why this even happens */

		memset(name, 0, 64);
		msg.ReadString(name, 64);
		score = msg.ReadLong();
		duration = msg.ReadFloat();

		query.response->AddPlayerToList(name, score, duration);
	}

	query.response->PlayersRefreshComplete();
	return true;
}

/*
 * Process server rules
*/
bool CServerCommunication::ProcessServerRules(scquery_t& query)
{
	if (!query.response)
		return false;

	bf_read& msg = query.msg;
	int numRules = msg.ReadShort();
	if (!numRules) {
		query.response->RulesFailedToRespond();
		return false;
	}
	//DevWarning("ProcessServerRules for %s\n", query.addr.ToString());

	char name[64];
	char value[64];
	for (int i = 0; i < numRules; i++)
	{
		memset(name, 0, 64);
		memset(value, 0, 64);

		msg.ReadString(name, 64);
		msg.ReadString(value, 64);

		query.response->RulesResponded(query.addr, name, value);
		//DevMsg("Rule responded: %s = %s\n", name, value);
	}

	query.response->RulesRefreshComplete();
	return true;
}

/*
 * Initializes query with default values
*/
scquery_t CServerCommunication::InitQuery(const netadr_t& adr, uint type, IServerQueryResponse* response) {
	scquery_t query{};

	query.addr = adr;
	query.type = type;
	query.msg = bf_read();
	query.response = response;
	query.sendTime = Plat_FloatTime();
	query.ping = 0;

	return query;
}

/*
 * Update challenge for a single query
*/
void CServerCommunication::UpdateChallenge(int id, int new_challenge) {
	int challengeId = m_ChallengeList.Find(id);

	if (m_ChallengeList.IsValidIndex(challengeId))
		if (m_ChallengeList[challengeId] == -1)
			m_ChallengeList[challengeId] = new_challenge;
}

/*
 * Retry server query request
*/
void CServerCommunication::RetryRequest(int id) {
	byte request = 0;
	scquery_t& query = GetQuery(id);

	// check request type
	if (query.type == k_ePingServer)
		return;
	else if (query.type == k_ePlayerDetails)
		request = A2S_PLAYER_REQUEST;
	else
		request = A2S_RULES_REQUEST;

	char buffer[64];
	bf_write msg(buffer, sizeof(buffer));

	// Get our challenge
	int challengeId = m_ChallengeList.Find(id);
	int challenge = 0;
	if (m_ChallengeList.IsValidIndex(challengeId))
		challenge = m_ChallengeList[challengeId];
	else
		return;

	if (challenge == -1)
		return;

	msg.WriteLong(CONNECTIONLESS_HEADER);
	msg.WriteByte(request);
	msg.WriteLong(challenge);
	msg.WriteByte(0);

	m_pSocket->Send(query.addr, msg);
}

/*
 * Run timeout checks, and other things
*/
void CServerCommunication::QueryFrame() {
	double curtime = Plat_FloatTime();

	// walk the query list, looking for any server timeouts
	unsigned short idx = m_Queries.FirstInorder();
	while (m_Queries.IsValidIndex(idx))
	{
		scquery_t& query = m_Queries[idx];

		float timeout =
			query.type == k_eServerRules ? 3.5f :
			query.type == k_ePlayerDetails ? 2.5f :
			1.5f;

		timeout += m_Queries.Count() * 0.015f;

		if (curtime - query.sendTime > timeout)
		{
			if (query.retries < 2)
			{
				RetryRequest(idx);

				query.sendTime = curtime;
				query.retries++;
			}
			else
			{
				DevWarning("Query timed out! addr: %s, type %u, id %u\n", query.addr.ToString(), query.type, idx);	

				// get the next server now, since we're about to delete it from query list
				unsigned short nextidx = m_Queries.NextInorder(idx);

				// delete the query
				m_Queries.RemoveAt(idx);

				// move to next item
				idx = nextidx;
			}
		}
		else
		{
			// still waiting for server result
			idx = m_Queries.NextInorder(idx);
		}
	}
}