#include <cint_types>
#include <cstdint>	// kvoli PseudorandomNumbers generatoru
#include <fstream>


using std::ostream;
using std:cerr;
using std:endl;

typedef uint8_t	 TUInt8	;
typedef uint16_t TUInt16;
typedef TElem	 TUInt8	;


enum EArchitecture {
	EArchUnknownOrDefault,
	EArchAmd64,
	EArchArmV7E
};

const char[] PARAM_STR_AMD64	= "-amd64"	;
const char[] PARAM_STR_ARMV7E	= "-armv7E"	;



class PseudoRndIntsGen
{
public:
	PseudoRndIntsGen()				{  }
	virtual ~PseudoRndIntsGen()		{  }
	
	// virtual TUInt64 getPrndNo64()= 0;
	virtual TUInt32	getPrndNo32()	= 0;
	virtual TUInt16	getPrndNo16()	= 0;
	virtual TUInt8	getPrndNo8()	= 0;
	
	virtual size_t storeItems( size_t nCnt, TUInt16 target[] ) = 0;
	virtual size_t storeItems( size_t nCnt, TUInt8  target[] ) = 0;
	
};


class PsdrndGenLinFdbShftReg: public PseudoRndIntsGen
{
	const TUInt16 LFSR_FIB_INIT_SEED_DEFAULT = 0xABCDu;
	TUInt16 m_lfsr;
	
protected:
	
	// prebrane z https://en.wikipedia.org/wiki/Linear-feedback_shift_register
	//	a upravene
	
	uint16_t lfsr_fib( uint16_t nShiftCyclesCnt )
	{
		uint16_t lfsr = m_lfsr;		// uint16_t lfsr = start_state;
		uint16_t bit;				// Must be 16-bit to allow bit<<15 later in the code
		uint32_t result = 0;

		for ( uint32_t i = 0; i < nShiftCyclesCnt; ++i )
		{	// taps: 16 14 13 11; feedback polynomial: x^16 + x^14 + x^13 + x^11 + 1
			bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
			lfsr = (lfsr >> 1) | (bit << 15);
		}

		m_lfsr = lfsr;
		
		return lfsr;
	}

protected:
	TUInt32 getPrndNumber32impl() {
		TUInt16 i1 = lfsr_fib( 16 );
		TUInt16 i2 = lfsr_fib( 16 );
		TUInt32 result = i1 | (i2 << 16);
		return	result;
	}
	TUInt16 getPrndNumber16impl() {
		TUInt16 result = lfsr_fib( 16 );
		return	result;
	}
	TUInt8 getPrndNumber8impl() {
		TUInt16	result = lfsr_fib( 8 );
		return	static_cast< TUInt8 >( result >> 8 );
	}

public:
	PsdrndGenLinFdbShftReg( TUInt16 nInitSeed = LFSR_FIB_INIT_SEED_DEFAULT, TUint16 nInitRounds = 0 ): m_lfsr( nInitSeed )
	{
		if ( nInitRounds > 0 )
			lfsr_fib( nInitRounds );
	}
	
	~PsdrndGenLinFdbShftReg()
	{  }
	
	void setSeed( TUInt16 nNewSeed ) {
		m_lfsr = nNewSeed;
	}
	
	virtual TUInt32 getPrndNumber32() {
		return	getPrndNumber32impl();
	}
	virtual TUInt16 getPrndNumber16() {
		return	getPrndNumber16impl();
	}
	virtual TUInt8 getPrndNumber8() {
		return	getPrndNumber8impl();
	
	virtual size_t storeItems( size_t nCnt, TUInt16 target[] );
	virtual size_t storeItems( size_t nCnt, TUInt8 target[] );
	
};

virtual size_t PsdrndGenLinFdbShftReg::storeItems( size_t nCnt, TUInt16 target[] )
{
	for ( size_t i = 0; i < nCnt; ++i ) {
		target[ i ] = getPrndNumber16impl();
	}
	return nCnt;
}

virtual size_t PsdrndGenLinFdbShftReg::storeItems( size_t nCnt, TUInt8 target[] )
{
	size_t i = 0, nCnt2 = (nCnt & (~1));
	for (  ; i < nCnt2; i += 2 ) {
		TUInt16 nItem = getPrndNumber16impl();
		target[ i   ] = static_cast<TUInt8>( nItem & 0xFF );
		target[ i+1 ] = static_cast<TUInt8>( nItem >> 8   );
	}
	if ( nCnt & 1 ) {
		target[ i ] = getPrndNumber8impl();
	}
	// ++i;	// netreba
	
	return nCnt;
}


// prebrate z https://stackoverflow.com/questions/19347685/calculating-modbus-rtu-crc-16
//	a upravene
//	(radsej toto, lebo nepouziva ziadne tabulky; hoci s tabulkou by zase mohlo byt v nejakych kontextoch rychlejsie)

TUInt16 calcCrc16( const TUInt8 aBuf[], size_t nLen )
{
	TUInt16 crc = 0xFFFF;	// unsigned int crc = 0xFFFF;
	for ( size_t pos = 0; pos < nLen; ++pos )
	{
		crc ^= static_cast< TUInt16 >( aBuf[pos] );    // XOR byte into least sig. byte of crc
		
		for ( int i = 8; i != 0; i-- ) {    // Loop over each bit
			if (( crc & 0x0001) != 0 ) {    // If the LSB is set
				crc >>= 1;                // Shift right and XOR 0xA001
				crc ^= 0xA001;
			} else {                      // Else LSB is not set
				crc >>= 1;                // Just shift right
			}
		}
	}
	
	return crc;
}


// template< typename TInt, typename TUInt8, bool TbBigEndian >
class LibFunctions
{
	bool	m_bBigEndian;
	bool	m_bBigEndianCrc16;
	bool	m_bUse16bitFnc2Values;
	bool	m_bSeparateInputsForFncs;
	bool	m_bZeroFnc2InpHighByte;
	
	int		m_nInputItemsPerRecord;
	int		m_nFnc2LowByteOfs, m_nFnc2HighByteOfs;
	
public:
	typedef TUInt8	TElem	;
	typedef TUInt16	TCrc16	;
	
	// predpokladam, ze budu vsade inlinovane; inak by sa pouzili priamo atributy
	//	(podobne, ak by vadilo, ze sa v debug-builde neinlinuju alebo zle krokuju)
	int getRecordLength()			const { return m_bUse16bitFnc2Values ? 16 : 14; }
	int getInputItemsPerRecord()	const { return m_nInputItemsPerRecord	; }
	bool useBigEndian()				const { return m_bBigEndian				; }
	bool useBigEndianCrc16()		const { return m_bBigEndianCrc16		; }
	bool use16bitFunction2Values()	const { return m_bUse16bitFnc2Values	; }
	bool useSeparateInputsForFncs()	const { return m_bSeparateInputsForFncs	; }
	bool zeroFnc2InpHighByte()		const { return m_bZeroFnc2InpHighByte	; }
	
	void calcParams()
	{
		if ( useSeparateInputsForFncs() ) {
			if ( use16bitFunction2Values() ) {
				m_nInputItemsPerRecord = 3;
				if ( useBigEndian() ) {
					m_nFnc2LowByteOfs  = 1;
					m_nFnc2HighByteOfs = 2;
				} else {
					m_nFnc2LowByteOfs  = 2;
					m_nFnc2HighByteOfs = 1;
				}
			} else {
				m_nInputItemsPerRecord = 2;
				m_bZeroFnc2InpHighByte = true;
				m_nFnc2LowByteOfs = m_nFnc2HighByteOfs = 1;
			}
		} else {
			m_nInputItemsPerRecord = 1;
			m_bZeroFnc2InpHighByte = true;
			m_nFnc2LowByteOfs = m_nFnc2HighByteOfs = 0;
		}
	}
	
	// static TInt getParsedBinary( TUInt8 param ) {
	//	TInt result = 0;
	//	while ( param ) {
	//		result += param & 1;
	//		param >>= 1;
	//	}
	//	return result;
	//	
	// alternativne napr.:
	// static TInt getParsedBinary( TUInt8 param ) {
	//	TInt result = 0;
	//	for ( int i = 0; i < 8; ++i ) {
	//		result += param & 1;
	//		param >>= 1;
	//	}
	//	return result;
	//	}
	// alternativne napr. (menej závislé / lepšie paralelizovateľné po rozvinutí cyklu):
	static TUInt8 getParsedBinary( TUInt8 param ) {
		TUInt8 result = 0;
		for ( int i = 0; i < 8; ++i )
			result += (param >> i) & 1;
		return result;
	}
	// alebo (manualne rozvinuty cyklus) napr.:
	// return	
	//			((param     ) & 1)	+
	//			((param >> 1) & 1)	+
	//			((param >> 2) & 1)	+
	//			((param >> 3) & 1)	+
	//			((param >> 4) & 1)	+
	//			((param >> 5) & 1)	+
	//			((param >> 6) & 1)	+
	//			((param >> 7) & 1)		;
	}
	
//	static TUInt16 getTwoBytesShiftedBy1000( TUInt8 p0, TUInt8 p1 ) {
//		// return (p0 | (p1 << 8)) >> 1000;
//		return 0;
//	}
	
	static TUInt16 getTwoBytesShiftedBy1000( TUInt16 p ) {
		// return p >> 1000;
		return 0;
	}
	
	
	
	// version for using every item in the sequence as inputs to both functions
	static bool canProcessRecord( int nReadPos, int nItemsCnt ) {
		return nReadPos+2 < nItemsCnt;
	}
	bool isStreamOk( ostream &out ) const {
		return ! out.is_open() || !out.good();
	}
	
	
	// mohlo by stat za to 
	
	static int writeRecord( TElem nInp1, nOut1, nInp2, nOut2, TElem aOutBuff[], ostream &out, bool bBigEndianCrc16 = true )
	int processBufferItems( int nItemsCnt, const TElem anElems[], ostream &out )
	{
		TUInt8 aOutBuff[16] = { 0xAA, 0xBB,
									0x01, 0xE1, 0xFF, 0xF1,
									0x02, 0xE1, 0xE2, 0xFF, 0xF1, 0xF2,
									0xC1, 0xC2,
								0, 0 };
		// 0xAA 0xBB 0x01 VSTUPNY_BAJT_FUNKCIE_1 0xFF VYSTUPNY_BAJT_FUNKCIE_1 0x02 
		// VSTUPNY_BAJT_FUNKCIE_1 VSTUPNY_BAJT_FUNKCIE_2 0xFF
		// VYSTUPNY_BAJT_FUNKCIE_1 VYSTUPNY_BAJT_FUNKCIE_2 CRC_16
		
		TCrc16 nCrc16;
		int nWrittenCnt = 0, nReadPos = 0;
		
		if ( ! out.is_open() || out.fail() )
			return -1-nWrittenCnt;
		
		TElem nInp1, nOut1, nInp21, nInp22, nOut2b0, nOut2b1, nCrcB0, nCrcB1;
		TUInt16 nInp2, nOut2;
		
		for ( ; canProcessRecord( nReadPos, nItemsCnt ); ++nWrittenCnt, nReadPos += getInputItemsPerRecord() ) )
		{
			nInp1  = anElems[ nReadPos ];
			nInp21 = anElems[ nReadPos + getFnc2InputLowByteOfs() ];
			if ( zeroFnc2InpHighByte() ) {
				nInp22 = 0;
			} else {
				nInp22 = anElems[ nReadPos + getFnc2InputHighByteOfs() ];
			}
			
			nOut1 = getParsedBinary( nInp1 );
			nOut2 = getTwoBytesShiftedBy1000( nInp21, nInp22 );
			
			nOut2b0 = static_cast< TElem >( nOut2 &  0xFF );
			nOut2b1 = static_cast< TElem >( nOut2 >> 8    );
			
			// uz su nastavene vopred (pred tymto cyklom); pri ladeni problemov s hodnotami by sa mohlo odkomentovat
//			anOutBuff[ 0] = 0xAA;
//			anOutBuff[ 1] = 0xBB;
//			anOutBuff[ 2] = 0x01;
//			anOutBuff[ 4] = 0xFF;
//			anOutBuff[ 6] = 0x02;
			
			// z hladiska udrziavatelnosti by mohlo byt lepsie to tam davat podobne ako <<(...) , t.j. buf[ pos++ ] = value; (hoci navzajom zavisle a serializovane)
			
			anOutBuff[ 3] = nInp1;
			anOutBuff[ 5] = nOut1;
			anOutBuff[ 7] = nInp1;
			anOutBuff[ 8] = nInp2b0;
			
			int nCrcPos;
			if ( use16bitFunction2Values() ) {
				anOutBuff[ 9] = nInp2b1;
				anOutBuff[10] = 0xFF;
				anOutBuff[11] = nOut1;
				if ( useBigEndian() ) {
					anOutBuff[12] = nOut2b0;
					anOutBuff[13] = nOut2b1;
				} else {
					anOutBuff[12] = nOut2b1;
					anOutBuff[13] = nOut2b0;
				}
				nCrcPos = 14;
			} else {
				anOutBuff[ 9] = 0xFF;
				anOutBuff[10] = nOut1;
				anOutBuff[11] = nOut2b0;
				nCrcPos = 12;
			}
			
			nCrc16 = calcCrc16( anOutBuff, getCrcBufLength() );
			nCrcB0 = static_cast<TElem>( nCrc16 &  0xFF );
			nCrcB1 = static_cast<TElem>( nCrc16 >> 8    );
			if ( useBigEndianCrc16() ) {
				anOutBuff[ nCrcPos   ] = nCrcB0;
				anOutBuff[ nCrcPos+1 ] = nCrcB1;
			} else {
				anOutBuff[ nCrcPos   ] = nCrcB1;
				anOutBuff[ nCrcPos+1 ] = nCrcB0;
			}
			
			out.write( anOutBuff, getOutputRecordLength() );
			
			if ( !isStreamOk( out ) )
				return -1 - nWrittenCnt;
			
		}
		
		out.flush();
		
		return nWrittenCnt;
	}

};


//void testFunctions()
//{
//	
//}


int main( int argc, const char * const argv[] )
{
	if ( argc < 1 ) {
		cerr << "required parameter is missing {amd64|armv7E}" << endl;
		exit 1;
	}
	
	EArchitecture eArch = EArchUnknownOrDefault;
	
	// pri vacsom pocte rozoznavanych parametrov by sa asi pouzila hash_map< std::string, NejakyManipulator >
	if ( strcmp( argv[1], PARAM_STR_AMD64 ) == 0 ) {
		eArch = EArchAmd64;
	} else if ( strcmp( argv[1], PARAM_STR_ARMV7E ) == 0 ) {
		eArch = EArchArmV7E;
	}
	
	case ( eArch ) {
		case EArchAmd64:
		case EArchArmV7E:
		break;
		
		default:
			cerr << "error: architecture not defined" << endl;
			exit 2;
	};
	
	const char[] pcOutFname = "bin_data.out";
	if ( argc > 1 ) {
		pcOutFname = argv[2];
	}
	fstream	out;
	out.open( pcOutFname, ios::binary | ios:out );
	if ( ! out.is_open() || out.fail() ) {
		cerr << "error: failed to open file \"" << pcOutFname << "\" for writing!" << endl;
		exit 2;
	}
	
	case ( eArch ) {
		case EArchAmd64:
		case EArchArmV7E:
			
		break;
	};

}
