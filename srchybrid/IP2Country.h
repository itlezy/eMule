//EastShare Start - added by AndCycle, IP to Country

// by Superlexx, based on IPFilter by Bouc7
#pragma once
#include <atlcoll.h>

struct IPRange_Struct2{
	uint32          IPstart;
	uint32          IPend;
	CString			ShortCountryName;
	CString			MidCountryName;
	CString			LongCountryName;
};

//EastShare Start - added by AndCycle, IP to Country
enum IP2CountryNameSelection{

	IP2CountryName_DISABLE = 0,
	IP2CountryName_SHORT,
	IP2CountryName_MID,
	IP2CountryName_LONG
};
//EastShare End - added by AndCycle, IP to Country

#define DFLT_IP2COUNTRY_FILENAME_OLD  _T("ip-to-country.csv")//Commander - Added: IP2Country auto-updating
#define DFLT_IP2COUNTRY_FILENAME  _T("GeoIPCountryWhois.csv")

typedef CTypedPtrArray<CPtrArray, IPRange_Struct2*> CIP2CountryArray;

class CIP2Country
{
	public:
		CIP2Country(void);
		~CIP2Country(void);
		
		void	Load();
		void	Unload();

		//reset ip2country referense
		void	Reset();

		//refresh passive windows
		void	Refresh();

		IPRange_Struct2*	GetDefaultIP2Country() {return &defaultIP2Country;}

		bool	LoadFromFile();
		void	RemoveAllIPs();

		void	AddIPRange(uint32 IPfrom,uint32 IPto, const TCHAR* shortCountryName, const TCHAR* midCountryName,  const TCHAR* longCountryName);

		IPRange_Struct2*	GetCountryFromIP(uint32 IP);
		CString	GetCountryNameFromRef(IPRange_Struct2* m_structServerCountry);

		CString GetDefaultFilePath(int &iNewDfltFile) const;
	private:

		//check is program current running, if it's under init or shutdown, set to false
		bool	m_bRunning;

		struct	IPRange_Struct2 defaultIP2Country;

		CIP2CountryArray m_iplist;
};

//EastShare End - added by AndCycle, IP to Country
