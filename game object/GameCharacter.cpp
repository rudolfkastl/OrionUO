/****************************************************************************
**
** GameObject.cpp
**
** Copyright (C) September 2015 Hotride
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
*****************************************************************************
*/
//---------------------------------------------------------------------------
#include "stdafx.h"
//---------------------------------------------------------------------------
TGameCharacter::TGameCharacter(DWORD serial)
: TGameObject(serial), m_Hits(0), m_MaxHits(0), m_Sex(false), m_Direction(0),
m_Notoriety(0), m_CanChangeName(false), m_AnimationGroup(0xFF),
m_AnimationInterval(0), m_AnimationFrameCount(0), m_AnimationRepeat(false),
m_AnimationRepeatMode(1), m_AnimationDirection(false), m_AnimationFromServer(false),
m_MaxMana(0), m_MaxStam(0), m_Mana(0), m_Stam(0), m_OffsetX(0), m_OffsetY(0),
m_OffsetZ(0), m_LastStepTime(0), m_LastStepSoundTime(GetTickCount()), m_Race(0),
m_TimeToRandomFidget(GetTickCount() + RANDOM_FIDGET_ANIMATION_DELAY),
m_AfterStepDelay(0), m_StepSoundOffset(0), m_CorpseLink(0)
{
	RenderQueueIndex = 7; //������� ��������� ���������� (����� ���� ���������� �� ����� � ���������� Z �����������)
	m_WalkStack.Init(); //������������� �������� �����
}
//---------------------------------------------------------------------------
TGameCharacter::~TGameCharacter()
{
	//������ ������
	m_WalkStack.Clear();
	m_PaperdollTextTexture.Clear();

	//���� ������ ��������� - ������� ���
	TGump *gump = GumpManager->GetGump(m_Serial, 0, GT_STATUSBAR);
	if (gump != NULL)
		gump->UpdateFrame();
}
//---------------------------------------------------------------------------
void TGameCharacter::SetName(string val)
{
	m_Name = val;

	if (g_GameState >= GS_GAME)
	{
		char buf[256] = {0};
		sprintf(buf, "Ultima Online - %s (%s)", val.c_str(), ServerList.GetServerName().c_str());

		SetWindowTextA(g_hWnd, buf);
	}
}
//---------------------------------------------------------------------------
void TGameCharacter::SetPaperdollText(string val)
{
	m_PaperdollTextTexture.Clear(); //������� ��������
	m_PaperdollText = val;

	if (val.length()) //���� ����� ���� - ����������� ����� ��������
		FontManager->GenerateA(1, m_PaperdollTextTexture, val.c_str(), 0x0386, 185);
}
//---------------------------------------------------------------------------
int TGameCharacter::Draw(bool &mode, int &drawX, int &drawY, DWORD &ticks)
{
	if (mode)
	{
		if (m_TimeToRandomFidget < ticks)
			SetRandomFidgetAnimation();

#if UO_DEBUG_INFO!=0
		g_RenderedObjectsCountInGameWindow++;
#endif

		DWORD lastSBsel = g_StatusbarUnderMouse;

		if (!IsPlayer() && g_Player->Warmode && g_LastSelectedObject == m_Serial)
			g_StatusbarUnderMouse = Serial;

		AnimationManager->DrawCharacter(this, drawX, drawY, m_Z); //Draw character

		g_StatusbarUnderMouse = lastSBsel;

		/*if (goc->IsPlayer())
		{
		//g_GL.DrawPolygone(0x7F7F7F7F, g_PlayerRect.X, g_PlayerRect.Y, g_PlayerRect.Width, g_PlayerRect.Height);
		}*/

		DrawEffects(drawX, drawY, ticks);
	}
	else
	{
		if (AnimationManager->CharacterPixelsInXY(this, drawX, drawY, m_Z))
		{
			g_LastObjectType = SOT_GAME_OBJECT;
			g_LastSelectedObject = m_Serial;
			g_SelectedObject = this;
		}
	}

	return 0;
}
//---------------------------------------------------------------------------
void TGameCharacter::UpdateSex()
{
	//���������� ���� � ����������� �� ������� �������� ���������
	switch (m_Graphic)
	{
		case 0x0190:
		case 0x0192:
		{
			m_Sex = false;
			break;
		}
		case 0x0191:
		case 0x0193:
		{
			m_Sex = true;
			break;
		}
		default:
			break;
	}
}
//---------------------------------------------------------------------------
bool TGameCharacter::IsCorrectStep(WORD cx, WORD cy, WORD &x, WORD &y, BYTE &dir)
{
	switch (dir & 7)
	{
		case 0:
		{
			cy--;
			break;
		}
		case 1:
		{
			cx++;
			cy--;
			break;
		}
		case 2:
		{
			cx++;
			break;
		}
		case 3:
		{
			cx++;
			cy++;
			break;
		}
		case 4:
		{
			cy++;
			break;
		}
		case 5:
		{
			cx--;
			cy++;
			break;
		}
		case 6:
		{
			cx--;
			break;
		}
		case 7:
		{
			cx--;
			cy--;
			break;
		}
	}

	return (cx == x && cy == y);
}
//---------------------------------------------------------------------------
bool TGameCharacter::IsTeleportAction(WORD &x, WORD &y, BYTE &dir)
{
	bool result = false;

	TWalkData *wd = m_WalkStack.m_Items;
	
	WORD cx = X;
	WORD cy = Y;
	BYTE cdir = Direction;

	if (wd != NULL)
	{
		while (wd->m_Next != NULL)
			wd = wd->m_Next;
		
		cx = wd->X;
		cy = wd->Y;
		cdir = wd->Direction;
	}

	if ((cdir & 7) == (dir & 7))
		result = !IsCorrectStep(cx, cy, x, y, dir);

	return result;
}
//---------------------------------------------------------------------------
void TGameCharacter::SetAnimation(BYTE id, BYTE interval, BYTE frameCount, BYTE repeatCount, bool repeat, bool frameDirection)
{
	m_AnimationGroup = id;
	m_AnimIndex = 0;
	m_AnimationInterval = interval;
	m_AnimationFrameCount = frameCount;
	m_AnimationRepeatMode = repeatCount;
	m_AnimationRepeat = repeat;
	m_AnimationDirection = frameDirection;
	m_AnimationFromServer = false;

	m_LastAnimationChangeTime = GetTickCount();
	m_TimeToRandomFidget = GetTickCount() + RANDOM_FIDGET_ANIMATION_DELAY;
}
//---------------------------------------------------------------------------
void TGameCharacter::SetRandomFidgetAnimation()
{
	m_AnimIndex = 0;
	m_AnimationFrameCount = 0;
	m_AnimationInterval = 1;
	m_AnimationRepeatMode = 1;
	m_AnimationDirection = true;
	m_AnimationRepeat = false;
	m_AnimationFromServer = true;

	m_TimeToRandomFidget = GetTickCount() + RANDOM_FIDGET_ANIMATION_DELAY;

	ANIMATION_GROUPS groupIndex = AnimationManager->GetGroupIndex(GetMountAnimation());

	const BYTE fidgetAnimTable[3][3] =
	{
		{LAG_FIDGET_1, LAG_FIDGET_2, LAG_FIDGET_1},
		{HAG_FIDGET_1, HAG_FIDGET_2, HAG_FIDGET_1},
		{PAG_FIDGET_1, PAG_FIDGET_2, PAG_FIDGET_3}
	};

	m_AnimationGroup = fidgetAnimTable[groupIndex - 1][RandomInt(3)];
}
//---------------------------------------------------------------------------
void TGameCharacter::SetAnimationGroup(BYTE val)
{
	m_AnimationFrameCount = 0;
	m_AnimationInterval = 0;
	m_AnimationRepeat = false;
	m_AnimationRepeatMode = 0;
	m_AnimationDirection = false;
	m_AnimationFromServer = false;

	/*if (val == 0xFF)
	{
		ANIMATION_GROUPS groupIndex = AnimationManager->GetGroupIndex(GetMountAnimation());

		if (groupIndex == AG_LOW)
		{
		}
		else if (groupIndex == AG_HIGHT)
		{
		}
		else if (groupIndex == AG_PEOPLE)
		{
		}
	}*/

	m_AnimationGroup = val;
}
//---------------------------------------------------------------------------
void TGameCharacter::GetAnimationGroup(ANIMATION_GROUPS group, BYTE &animation)
{
	const BYTE animTableLO[35] =
	{
		LAG_WALK,
		LAG_WALK,
		LAG_RUN,
		LAG_RUN,
		LAG_STAND,
		LAG_FIDGET_1,
		LAG_FIDGET_2,
		LAG_STAND,
		LAG_STAND,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_WALK,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_DIE_1,
		LAG_DIE_2,
		LAG_WALK,
		LAG_RUN,
		LAG_STAND,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_EAT,
		LAG_WALK,
		LAG_EAT,
		LAG_EAT,
		LAG_FIDGET_1
	};

	const BYTE animTableHI[35] =
	{
		HAG_WALK,
		HAG_WALK,
		HAG_WALK,
		HAG_WALK,
		HAG_STAND,
		HAG_FIDGET_1,
		HAG_FIDGET_2,
		HAG_ATTACK_1,
		HAG_ATTACK_2,
		HAG_ATTACK_3,
		HAG_ATTACK_1,
		HAG_ATTACK_2,
		HAG_ATTACK_3,
		HAG_ATTACK_1,
		HAG_ATTACK_2,
		HAG_WALK,
		HAG_CAST,
		HAG_CAST,
		HAG_ATTACK_1,
		HAG_ATTACK_2,
		HAG_GET_HIT_1,
		HAG_DIE_1,
		HAG_DIE_2,
		HAG_WALK,
		HAG_WALK,
		HAG_STAND,
		HAG_ATTACK_1,
		HAG_ATTACK_2,
		HAG_ATTACK_1,
		HAG_ATTACK_2,
		HAG_STAND,
		HAG_WALK,
		HAG_STAND,
		HAG_STAND,
		HAG_FIDGET_1
	};

	switch (group)
	{
		case AG_LOW:
		{
			if (animation < 35)
				animation = animTableLO[animation];

			break;
		}
		case AG_HIGHT:
		{
			if (animation < 35)
				animation = animTableHI[animation];

			break;
		}
		default:
			break;
	}
}
//---------------------------------------------------------------------------
BYTE TGameCharacter::GetAnimationGroup(WORD graphic)
{
	ANIMATION_GROUPS groupIndex = AG_LOW;
	BYTE result = m_AnimationGroup;

	if (graphic)
	{
		groupIndex = AnimationManager->GetGroupIndex(graphic);

		if (result != 0xFF)
			GetAnimationGroup(groupIndex, result);
	}
	else
		groupIndex = AnimationManager->GetGroupIndex(GetMountAnimation());

	bool isWalking = Walking();
	bool isRun = (m_Direction & 0x80);

	if (!m_WalkStack.Empty())
	{
		isWalking = true;
		isRun = m_WalkStack.m_Items->Run();
	}

	if (groupIndex == AG_LOW)
	{
		if (isWalking)
		{
			if (isRun)
				result = (BYTE)LAG_RUN;
			else
				result = (BYTE)LAG_WALK;
		}
		else if (m_AnimationGroup == 0xFF)
			result = (BYTE)LAG_STAND;
	}
	else if (groupIndex == AG_HIGHT)
	{
		if (isWalking)
			result = (BYTE)HAG_WALK;
		else if (m_AnimationGroup == 0xFF)
			result = (BYTE)HAG_STAND;
	}
	else if (groupIndex == AG_PEOPLE)
	{
		bool InWar = InWarMode();

		if (Serial == g_PlayerSerial)
			InWar = g_Player->Warmode;

		if (isWalking)
		{
			if (isRun)
			{
				if (FindLayer(OL_MOUNT) != NULL)
					result = (BYTE)PAG_ONMOUNT_RIDE_FAST;
				else if (FindLayer(OL_1_HAND) != NULL || FindLayer(OL_2_HAND) != NULL)
					result = (BYTE)PAG_RUN_ARMED;
				else
					result = (BYTE)PAG_RUN_UNARMED;
			}
			else
			{
				if (FindLayer(OL_MOUNT) != NULL)
					result = (BYTE)PAG_ONMOUNT_RIDE_SLOW;
				else if (FindLayer(OL_1_HAND) != NULL || FindLayer(OL_2_HAND) != NULL)
				{
					if (InWar)
						result = (BYTE)PAG_WALK_WARMODE;
					else
						result = (BYTE)PAG_WALK_ARMED;
				}
				else if (InWar)
					result = (BYTE)PAG_WALK_WARMODE;
				else
					result = (BYTE)PAG_WALK_UNARMED;
			}
		}
		else if (m_AnimationGroup == 0xFF)
		{
			if (FindLayer(OL_MOUNT) != NULL)
				result = (BYTE)PAG_ONMOUNT_STAND;
			else if (InWar)
			{
				if (FindLayer(OL_1_HAND) != NULL)
					result = (BYTE)PAG_STAND_ONEHANDED_ATTACK;
				else if (FindLayer(OL_2_HAND) != NULL)
					result = (BYTE)PAG_STAND_TWOHANDED_ATTACK;
				else
					result = (BYTE)PAG_STAND_ONEHANDED_ATTACK;
			}
			else
				result = (BYTE)PAG_STAND;
		}
	}

	return result;
}
//---------------------------------------------------------------------------
WORD TGameCharacter::GetMountAnimation()
{
	WORD graphic = m_Graphic;

	switch (graphic)
	{
		case 0x0192: //male ghost
		case 0x0193: //female ghost
		{
			graphic -= 2;

			break;
		}
		default:
			break;
	}
	
	return graphic;
}
//---------------------------------------------------------------------------
void TGameCharacter::UpdateAnimationInfo(BYTE &dir, bool canChange)
{
	dir = m_Direction;
	if (dir & 0x80)
		dir ^= 0x80;

	TWalkData *wd = m_WalkStack.m_Items;

	if (wd != NULL)
	{
		DWORD ticks = GetTickCount();

		m_TimeToRandomFidget = ticks + RANDOM_FIDGET_ANIMATION_DELAY;
		
		dir = wd->Direction;
		int run = 0;

		if (dir & 0x80)
		{
			dir ^= 0x80;
			run = 1;
		}

		if (canChange)
		{
			int onMount = (int)(FindLayer(OL_MOUNT) != NULL);

			int maxDelay = g_CharacterAnimationDelayTable[onMount][run];

			int delay = (int)ticks - (int)m_LastStepTime;
			bool removeStep = (delay >= maxDelay);

			if (X != wd->X || Y != wd->Y)
			{
				float steps = maxDelay / g_AnimCharactersDelayValue;
				
				float x = (delay / g_AnimCharactersDelayValue);
				float y = x;
				
				wd->GetOffset(x, y, steps);

				m_OffsetX = (char)x;
				m_OffsetY = (char)y;

				m_OffsetZ = 0;
			}
			else
			{
				m_OffsetX = 0;
				m_OffsetY = 0;
				m_OffsetZ = 0;

				removeStep = (delay >= 100); //direction change
			}

			if (removeStep)
			{
				m_X = wd->X;
				m_Y = wd->Y;
				m_Z = wd->Z;
				m_Direction = wd->Direction;

				m_AfterStepDelay = maxDelay / 3;

				m_OffsetX = 0;
				m_OffsetY = 0;
				m_OffsetZ = 0;

				m_WalkStack.Pop();

				MapManager->AddRender(this);
				//World->MoveToTop(this);

				m_LastStepTime = ticks;
			}
		}
	}
	else
	{
		m_OffsetX = 0;
		m_OffsetY = 0;
		m_OffsetZ = 0;
	}
}
//---------------------------------------------------------------------------
TGameItem *TGameCharacter::FindLayer(int layer)
{
	TGameItem *item = NULL;

	for (TGameObject *obj = (TGameObject*)m_Items; obj != NULL; obj = (TGameObject*)obj->m_Next)
	{
		if (((TGameItem*)obj)->Layer == layer)
		{
			item = (TGameItem*)obj;
			break;
		}
	}

	return item;
}
//---------------------------------------------------------------------------