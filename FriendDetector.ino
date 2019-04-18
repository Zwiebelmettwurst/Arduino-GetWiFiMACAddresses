#include "./esppl_functions.h"
#include <cassert>
#include <cstdio>
#include <stdint.h>

void stop(void)
{
  Serial.printf("stop, but no stop\n");
}

bool compareMac(const uint8_t *a, const uint8_t *b)
{
  for (int i{ 0 }; i < ESPPL_MAC_LEN; ++i)
  {
    if (a[i] != b[i])
    {
      return false;
    }
  }

  return true;
}

class CUser
{
private:
  uint8_t m_arrMac[ESPPL_MAC_LEN]{};
  unsigned long m_ulLastSeen{};

public:
  unsigned long getLastSeen(void) const
  {
    return this->m_ulLastSeen;
  }

  const uint8_t *getMac(char *szOut = nullptr) const
  {
    if (szOut != nullptr)
    {
      sprintf(szOut, "%02X%02X%02X%02X%02X%02X",
               this->m_arrMac[0],
               this->m_arrMac[1],
               this->m_arrMac[2],
               this->m_arrMac[3],
               this->m_arrMac[4],
               this->m_arrMac[5]);
    }

    return this->m_arrMac;
  }

  void see(void)
  {
    this->m_ulLastSeen = millis();
  }

  bool operator==(const CUser &that)
  {
    if (this->m_ulLastSeen != that.m_ulLastSeen)
    {
      return false;
    }

    if (!compareMac(this->m_arrMac, that.m_arrMac))
    {
      return false;
    }

    return true;
  }

  CUser(const uint8_t *arrMac)
  {
    for (int i{ 0 }; i < ESPPL_MAC_LEN; ++i)
    {
      this->m_arrMac[i] = arrMac[i];
    }

    this->see();
  }

  CUser(void)
  {
    this->see();
  }
};

template <class T, int k_iCapacity>
class CSimpleList
{
public:
  static constexpr int m_iMaxSize{ k_iCapacity };

private:
  int m_iLength{ 0 };
  T *m_arrData{ new T[m_iMaxSize]{} };

public:
  int size(void) const
  {
    return this->m_iLength;
  }

  T &get(int iIndex)
  {
    if ((iIndex < 0) || (iIndex >= this->m_iLength))
    {
      Serial.printf("index out of bounds\n");
      stop();
    }

    return this->m_arrData[iIndex];
  }

  bool add(const T &el)
  {
    if (this->m_iLength >= this->m_iMaxSize)
    {
      // Serial.printf("list capacity exceeded");
      return false;
    }
    else
    {
      this->m_arrData[this->m_iLength++] = el;
      return true;
    }
  }

  void remove(int iIndex)
  {
    if ((iIndex < 0) || (iIndex >= this->m_iLength))
    {
      Serial.printf("index out of bounds (remove)\n");
      stop();
    }

    if (this->m_iLength <= 0)
    {
      Serial.printf("tried removing an element from an empty list");
    }
    else
    {
      --this->m_iLength;
    }
  }

  bool has(const T &el) const
  {
    for (int i{ 0 }; i < this->m_iLength; ++i)
    {
      if (this->m_arrData[i] == el)
      {
        return true;
      }
    }

    return false;
  }

  ~CSimpleList(void)
  {
    delete[] this->m_arrData;
  }
};

CSimpleList<CUser, 400> list{};

/**
 * Millisecond(s)
 */
constexpr unsigned long k_ulCheckInterval{ 1 * (60 * 60 * 1000) };
constexpr unsigned long k_ulCooldown{ 1 * (60 * 60 * 1000) };
bool startsend{ false };

bool arrayContains(int *arr, int iLength, int iElement)
{
  for (int i{ 0 }; i < iLength; ++i)
  {
    if (arr[i] == iElement)
    {
      return true;
    }
  }

  return false;
}

void clearOld(void)
{
  int iOldIdLength{ 0 };
  int arrOldIds[decltype(list)::m_iMaxSize / 2]{};

  int iOriginalListSize{ list.size() / 2 };

  for (int i{ 0 }; i < iOriginalListSize; ++i)
  {
    int iOldestIndex{ 0 };
    unsigned long ulOldest{ 0 };

    for (int j{ 0 }; j < list.size(); ++j)
    {
      if (arrayContains(arrOldIds, iOldIdLength, j))
      {
        continue;
      }

      unsigned long iLastSeen{ list.get(j).getLastSeen() };

      if ((ulOldest == 0) || (iLastSeen > ulOldest))
      {
        ulOldest = iLastSeen;
        iOldestIndex = j;
      }
    }

    if (ulOldest == 0)
    {
      break;
    }
    else
    {
      arrOldIds[iOldIdLength++] = iOldestIndex;
      list.remove(iOldestIndex);
    }
  }
}

void cb(esppl_frame_info *info)
{
  CUser *pFound{ nullptr };

  for (int i{ 0 }; i < list.size(); ++i)
  {
    if (compareMac(list.get(i).getMac(), info->sourceaddr))
    {
      pFound = &list.get(i);
    }
  }

  if (pFound != nullptr)
  {
    pFound->see();
  }
  else
  {
    if (!list.add({ info->sourceaddr }))
    {
      Serial.printf("list capacity exceeded, clearing old users, adding new one.");
      clearOld();

      list.add({ info->sourceaddr });
    }

    char szMac[12]{};

    list.get(list.size() - 1).getMac(szMac);

    Serial.printf("MAC:%s\n", szMac);
  }
}

void setup()
{
  delay(500);

  Serial.begin(115200);
  list.get(1);

  //serial.available()
  //wenn serial input, dann fang an.
  esppl_init(cb);

  esppl_sniffing_start();
}

void loop()
{
  static unsigned long s_ulNextCheck{ 0 };

  if (Serial.available() > 0 && list.size() > 0)
  {
    int incomingByte{ Serial.read() }; // read the incoming byte:

    if (incomingByte == 'f')
    {
      Serial.printf("MACLIST:");

      for (int i{ 0 }; i < list.size(); ++i)
      {
        char szMac[12]{};

        list.get(i).getMac(szMac);

        Serial.printf("%s#", szMac);
      }

      Serial.printf("\n");
    }

    //  Serial.print(" I received:");

    //Serial.println(incomingByte);
  }

  unsigned long ulTime{ millis() };

  if (ulTime >= s_ulNextCheck)
  {
    s_ulNextCheck = (ulTime + k_ulCheckInterval);

    for (int i{ 0 }; i < list.size(); ++i)
    {
      const CUser &user{ list.get(i) };

      if (user.getLastSeen() < (ulTime - k_ulCooldown))
      {
        // Serial.printf("removing a user\n");

        list.remove(i);
      }
    }
  }

  for (int i{ ESPPL_CHANNEL_MIN }; i <= ESPPL_CHANNEL_MAX; ++i)
  {
    esppl_set_channel(i);
    while (esppl_process_frames())
      ;
  }
}
