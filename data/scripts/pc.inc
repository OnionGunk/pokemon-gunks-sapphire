EventScript_PC:: @ 81A0009
	lockall
	setvar VAR_0x8004, 0
	special DoPCTurnOnEffect
	playse SE_PC_ON
	msgbox Text_BootUpPC, MSGBOX_DEFAULT
	goto EventScript_PCMainMenu
	end

EventScript_PCMainMenu:
	message Text_WhichPCShouldBeAccessed
	waitmessage
	special ScriptMenu_CreatePCMultichoice
	waitstate
	goto EventScript_AccessPC
	end

EventScript_AccessPC:
	switch VAR_RESULT
	case 0, EventScript_AccessPokemonStorage
	case 1, EventScript_AccessPlayersPC
	case 2, EventScript_GunkMoveTutor
	case 3, EventScript_AccessHallOfFame
	case 4, EventScript_TurnOffPC
	case MULTI_B_PRESSED, EventScript_TurnOffPC
	end

EventScript_AccessPlayersPC:
	playse SE_PC_LOGIN
	msgbox Text_AccessedPlayersPC, MSGBOX_DEFAULT
	special PlayerPC
	waitstate
	goto EventScript_PCMainMenu
	end

EventScript_AccessPokemonStorage:
	playse SE_PC_LOGIN
	call_if_unset FLAG_SYS_PC_LANETTE, EventScript_AccessSomeonesPC
	call_if_set FLAG_SYS_PC_LANETTE, EventScript_AccessLanettesPC
	msgbox Text_StorageSystemOpened, MSGBOX_DEFAULT
	special ShowPokemonStorageSystem
	waitstate
	goto EventScript_PCMainMenu
	end

EventScript_AccessSomeonesPC:
	msgbox Text_AccessedSomeonesPC, MSGBOX_DEFAULT
	return

EventScript_AccessLanettesPC:
	msgbox Text_AccessedLanettesPC, MSGBOX_DEFAULT
	return

EventScript_TurnOffPC:
	setvar VAR_0x8004, 0
	playse SE_PC_OFF
	special DoPCTurnOffEffect
	releaseall
	end

EventScript_AccessHallOfFame:
	goto_if_unset FLAG_SYS_GAME_CLEAR, EventScript_TurnOffPC
	playse SE_PC_LOGIN
	special AccessHallOfFamePC
	waitstate
	goto EventScript_AccessPC
	end

@ Gunk: a free Move Tutor in every PC (Fallarbor's relearner without the Heart Scale, Lilycove's deleter)
EventScript_GunkMoveTutor:
	playse SE_PC_LOGIN
	message Text_GunkMoveTutorWhat
	waitmessage
	multichoice 0, 0, MULTI_GUNK_MOVE_TUTOR, 0
	switch VAR_RESULT
	case 0, EventScript_GunkRemember
	case 1, EventScript_GunkForget
	goto EventScript_PCMainMenu
	end

EventScript_GunkRemember:
	msgbox FallarborTown_MoveRelearnersHouse_Text_TutorWhichMon, MSGBOX_DEFAULT
	special SelectMoveTutorMon
	waitstate
	compare VAR_0x8004, 255
	goto_if_eq EventScript_GunkMoveTutor
	special IsSelectedMonEgg
	compare VAR_RESULT, 1
	goto_if_eq EventScript_GunkRememberEgg
	compare VAR_0x8005, 0
	goto_if_eq EventScript_GunkRememberNone
	msgbox FallarborTown_MoveRelearnersHouse_Text_TeachWhichMove, MSGBOX_DEFAULT
	special DisplayMoveTutorMenu
	waitstate
	goto EventScript_GunkMoveTutor
	end

EventScript_GunkRememberEgg:
	msgbox FallarborTown_MoveRelearnersHouse_Text_CantTeachEgg, MSGBOX_DEFAULT
	goto EventScript_GunkMoveTutor
	end

EventScript_GunkRememberNone:
	msgbox FallarborTown_MoveRelearnersHouse_Text_DontHaveMoveToTeachPokemon, MSGBOX_DEFAULT
	goto EventScript_GunkMoveTutor
	end

EventScript_GunkForget:
	msgbox LilycoveCity_MoveDeletersHouse_Text_WhichMonShouldForget, MSGBOX_DEFAULT
	special SelectMonForNPCTrade
	waitstate
	compare VAR_0x8004, 255
	goto_if_eq EventScript_GunkMoveTutor
	special IsSelectedMonEgg
	compare VAR_RESULT, 1
	goto_if_eq EventScript_GunkForgetEgg
	special ScrSpecial_CountPokemonMoves
	compare VAR_RESULT, 1
	goto_if_eq EventScript_GunkForgetOnlyOne
	msgbox LilycoveCity_MoveDeletersHouse_Text_WhichMoveShouldBeForgotten, MSGBOX_DEFAULT
	fadescreen FADE_TO_BLACK
	special SelectMove
	fadescreen FADE_FROM_BLACK
	compare VAR_0x8005, 4
	goto_if_eq EventScript_GunkForget
	special ScrSpecial_GetPokemonNicknameAndMoveName
	msgbox LilycoveCity_MoveDeletersHouse_Text_MonsMoveShouldBeForgotten, MSGBOX_YESNO
	compare VAR_RESULT, NO
	goto_if_eq EventScript_GunkMoveTutor
	special DeleteMonMove
	playfanfare MUS_MOVE_DELETED
	waitfanfare
	msgbox LilycoveCity_MoveDeletersHouse_Text_MonHasForgottenMove, MSGBOX_DEFAULT
	goto EventScript_GunkMoveTutor
	end

EventScript_GunkForgetOnlyOne:
	special ScrSpecial_GetPokemonNicknameAndMoveName
	msgbox LilycoveCity_MoveDeletersHouse_Text_MonOnlyKnowsOneMove, MSGBOX_DEFAULT
	goto EventScript_GunkMoveTutor
	end

EventScript_GunkForgetEgg:
	msgbox LilycoveCity_MoveDeletersHouse_Text_EggCantForgetMoves, MSGBOX_DEFAULT
	goto EventScript_GunkMoveTutor
	end

Text_GunkMoveTutorWhat:
	.string "MOVE TUTOR: remember or forget a\n"
	.string "move?$"
