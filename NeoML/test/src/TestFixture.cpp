/* Copyright © 2017-2024 ABBYY

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
--------------------------------------------------------------------------------------------------------------*/

#include <common.h>
#pragma hdrstop

#include <TestFixture.h>
#include <algorithm>
#include <memory>

namespace NeoMLTest {

namespace {

	IMathEngine* mathEngine = nullptr;
	CString testDir{};
	void* platformEnv = nullptr;
	TMathEngineType type = MET_Undefined;

	template <typename T, std::size_t N>
	bool startsWith( const T* str, const T(&prefix)[N] )
	{
		size_t i = 0;
		for( ; i < N && *str != '\0'; ++i, ++str ) {
			if( *str != prefix[i] ) {
				return false;
			}
		}
		return i == N;
	}

	template <typename T, std::size_t N>
	bool equal( const T* one, const T(&two)[N] )
	{
		return startsWith( one, two ) && one[N] == '\0';
	}

	template <typename T, std::size_t N>
	const T* argValue( int argc, T* argv[], const T(&argument)[N] )
	{
		for( int i = 0; i < argc; ++i ) {
			if( startsWith( argv[i], argument ) ) {
				return argv[i] + N;
			}
		}
		return nullptr;
	}

#ifdef NEOML_USE_FINEOBJ
    using TCharType = wchar_t;
#else
	using TCharType = char;
#endif

	static constexpr TCharType TestDataPath[]{ '-','-','T','e','s','t','D','a','t','a','P','a','t','h','=' };
	static constexpr TCharType MathEngineArg[]{ '-','-','M','a','t','h','E','n','g','i','n','e','=' };
	static constexpr TCharType Cpu[]{ 'c', 'p', 'u' };
	static constexpr TCharType Cuda[]{ 'c','u','d','a' };
	static constexpr TCharType Vulkan[]{ 'v','u','l','k','a','n' };
	static constexpr TCharType Metal[]{ 'm','e','t','a','l' };

	static TMathEngineType getMathEngineType( int argc, TCharType* argv[] )
	{
		auto value = argValue( argc, argv, MathEngineArg );
		if( !value ) {
			return MET_Undefined;
		}

		if( equal( value, Cpu ) ) {
			return MET_Cpu;
		} else if( equal( value, Metal ) ) {
			return MET_Metal;
		} else if( equal( value, Cuda ) ) {
			return MET_Cuda;
		} else if( equal( value, Vulkan ) ) {
			return MET_Vulkan;
		}
		return MET_Undefined;
	}

	static const char* toString( TMathEngineType type )
	{
		if( type == MET_Cpu ) {
			return "Cpu";
		} else if( type == MET_Cuda ) {
			return "Cuda";
		} else if( type == MET_Vulkan ) {
			return "Vulkan";
		} else if( type == MET_Metal ) {
			return "Metal";
		}
		return "";
	}

	static void initTestDataPath( int argc, TCharType* argv[] )
	{
		auto value = argValue( argc, argv, TestDataPath );
		if( value ) {
#ifdef NEOML_USE_FINEOBJ
			testDir = CString( value, CP_UTF8 );
#else  //NEOML_USE_FINEOBJ
			testDir = FObj::CString( value );
#endif //NEOML_USE_FINEOBJ
		}
	}

} // end namespace

//------------------------------------------------------------------------------------------------------------

double GetTimeScaled( IPerformanceCounters& counters, int scale )
{
	return ( double( counters[0].Value ) / scale );
}

double GetPeakMemScaled( IMathEngine& mathEngine, int scale )
{
	return ( double( mathEngine.GetPeakMemoryUsage() ) / scale );
}

IMathEngine* CreateMathEngine( TMathEngineType type, std::size_t memoryLimit )
{
	IMathEngine* result = nullptr;
	switch( type ) {
		case MET_Cuda:
		case MET_Vulkan:
		case MET_Metal: {
			std::unique_ptr<IGpuMathEngineManager> gpuManager( CreateGpuMathEngineManager() );
			CMathEngineInfo info;
			for( int i = 0; i < gpuManager->GetMathEngineCount(); ++i ) {
				gpuManager->GetMathEngineInfo( i, info );
				if( info.Type == type ) {
					result = gpuManager->CreateMathEngine( i, memoryLimit );
					break;
				}
			}
			if( result ) {
				GTEST_LOG_( INFO ) << "Create GPU " << toString( type ) << " MathEngine: " << info.Name;
			} else {
				GTEST_LOG_( ERROR ) << "Can't create GPU " << toString( type ) << " MathEngine!";
			}
			break;
		}
		case MET_Undefined:
			GTEST_LOG_( WARNING ) << "Unknown type of MathEngine!";
			// fall through
		case MET_Cpu: {
			result = CreateCpuMathEngine( memoryLimit );
			GTEST_LOG_( INFO ) << "Create CPU MathEngine, threadCount = 1";
			break; 
		}
		default:
			break;
	}
	return result;
}

#ifdef NEOML_USE_FINEOBJ
int RunTests( int argc, wchar_t* argv[], void* platformEnv )
#else
int RunTests( int argc, char* argv[], void* platformEnv )
#endif
{
	NeoMLTest::initTestDataPath( argc, argv );
	::testing::InitGoogleTest( &argc, argv );

	type = getMathEngineType( argc, argv );

	SetPlatformEnv( platformEnv );

	int result = RUN_ALL_TESTS();

	DeleteMathEngine();

	return result;
}

void SetPlatformEnv( void* _platformEnv )
{
	platformEnv = _platformEnv;
}

void* GetPlatformEnv()
{
	return platformEnv;
}

//------------------------------------------------------------------------------------------------------------

static inline bool isPathSeparator( char ch ) { return ch == '\\' || ch == '/'; }

static inline char pathSeparator()
{
#if FINE_PLATFORM( FINE_WINDOWS )
	return '\\';
#elif FINE_PLATFORM( FINE_LINUX ) || FINE_PLATFORM( FINE_ANDROID ) || FINE_PLATFORM( FINE_IOS ) || FINE_PLATFORM( FINE_DARWIN )
	return '/';
#else
#error Unknown platform
#endif
}

static CString mergePathSimple( const CString& dir, const CString& relativePath )
{
	const char* dirPtr = dir;
	const size_t dirLen = strlen( dirPtr );
	const char* relativePathPtr = relativePath;
	const size_t relativePathLen = strlen( relativePathPtr );
	if( dirLen == 0 ) {
		return relativePath;
	}

	int separatorsCount = 0;
	if( isPathSeparator( dirPtr[dirLen - 1] ) ) {
		separatorsCount++;
	}
	if( relativePathLen > 0 && isPathSeparator( relativePathPtr[0] ) ) {
		separatorsCount++;
	}

	CString result{};
	switch( separatorsCount ) {
		case 0:
			result = dir + pathSeparator();
			break;
		case 1:
			result = dir;
			break;
		case 2:
			result = CString( dirPtr, static_cast<int>( dirLen - 1 ) );
			break;
		default:
			NeoAssert( false );
	}
	return result + relativePath;
}

//------------------------------------------------------------------------------------------------------------

CString GetTestDataFilePath( const CString& relativePath, const CString& fileName )
{
	return mergePathSimple( mergePathSimple( testDir, relativePath ), fileName );
}

IMathEngine& MathEngine()
{
	if( mathEngine == nullptr ) {
		mathEngine = CreateMathEngine( type, /*memoryLimit*/0u );
		NeoAssert( mathEngine != nullptr );
		SetMathEngineExceptionHandler( GetExceptionHandler() );
	}
	return *mathEngine;
}

void DeleteMathEngine()
{
	if( mathEngine ) {
		delete mathEngine;
		mathEngine = nullptr;
	}
}

TMathEngineType MathEngineType()
{
	return type;
}

} // namespace NeoMLTest
