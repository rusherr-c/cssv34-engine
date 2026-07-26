//=============================================================================
// Purpose: Holds all the protocol bits and defines used in master networking
//

#ifndef TRACKERPROTOCOL_H
#define TRACKERPROTOCOL_H
#ifdef _WIN32
#pragma once
#endif

#define M2C_QUERY				'f'
#define C2M_CLIENTQUERY			'1'

#define A2S_INFO_REQUEST		'T'
#define S2A_INFO_REPLY			'I'

#define A2S_PLAYER_REQUEST		'U'
#define S2A_PLAYER_REPLY		'D'

#define A2S_RULES_REQUEST		'V'
#define S2A_RULES_REPLY			'E'

#endif // TRACKERPROTOCOL_H