#include "third_party/doctest/doctest.h"
#include "TestSupport.h"
#include "../Ring.h"
#include <vector>

/**
 * Builds a deterministic sample entry for CRing tests.
 */
static TransferredData MakeTransferredData(uint64 nDataLength, DWORD dwTimestamp)
{
	TransferredData data = { nDataLength, dwTimestamp };
	return data;
}

/**
 * Materializes the ring contents in logical order for assertions.
 */
static std::vector<uint64> GetDataLengths(const CRing<TransferredData> &ring)
{
	std::vector<uint64> values;
	values.reserve(static_cast<size_t>(ring.Count()));
	for (UINT_PTR index = 0; index < ring.Count(); ++index)
		values.push_back(ring[index].datalen);
	return values;
}

TEST_CASE("CRing stores the first element after construction")
{
	CRing<TransferredData> ring(4, 4);

	ring.AddTail(MakeTransferredData(11, 7));

	CHECK_EQ(ring.Count(), 1);
	CHECK_FALSE(ring.IsEmpty());
	CHECK_EQ(ring.Head().datalen, static_cast<uint64>(11));
	CHECK_EQ(ring.Tail().timestamp, static_cast<DWORD>(7));
	CHECK_EQ(ring[0].datalen, static_cast<uint64>(11));
}

TEST_CASE("CRing restarts cleanly after RemoveAll")
{
	CRing<TransferredData> ring(4, 4);

	ring.AddTail(MakeTransferredData(1, 10));
	ring.AddTail(MakeTransferredData(2, 20));
	ring.RemoveAll();

	CHECK(ring.IsEmpty());
	CHECK_EQ(ring.Count(), 0);

	ring.AddTail(MakeTransferredData(99, 42));

	CHECK_EQ(ring.Count(), 1);
	CHECK_EQ(ring.Head().datalen, static_cast<uint64>(99));
	CHECK_EQ(ring.Tail().timestamp, static_cast<DWORD>(42));
}

TEST_CASE("CRing preserves logical order across wrap-around")
{
	CRing<TransferredData> ring(4, 4);

	ring.AddTail(MakeTransferredData(1, 10));
	ring.AddTail(MakeTransferredData(2, 20));
	ring.AddTail(MakeTransferredData(3, 30));
	ring.AddTail(MakeTransferredData(4, 40));
	ring.RemoveHead();
	ring.RemoveHead();
	ring.AddTail(MakeTransferredData(5, 50));
	ring.AddTail(MakeTransferredData(6, 60));

	CHECK(GetDataLengths(ring) == std::vector<uint64>{ 3, 4, 5, 6 });
	CHECK_EQ(ring.Head().timestamp, static_cast<DWORD>(30));
	CHECK_EQ(ring.Tail().timestamp, static_cast<DWORD>(60));
}

TEST_CASE("CRing preserves order when capacity grows from a wrapped state")
{
	CRing<TransferredData> ring(3, 2);

	ring.AddTail(MakeTransferredData(1, 10));
	ring.AddTail(MakeTransferredData(2, 20));
	ring.AddTail(MakeTransferredData(3, 30));
	ring.RemoveHead();
	ring.RemoveHead();
	ring.AddTail(MakeTransferredData(4, 40));
	ring.AddTail(MakeTransferredData(5, 50));
	ring.AddTail(MakeTransferredData(6, 60));

	CHECK_EQ(ring.Capacity(), static_cast<UINT_PTR>(5));
	CHECK(GetDataLengths(ring) == std::vector<uint64>{ 3, 4, 5, 6 });
	CHECK_EQ(ring.Head().timestamp, static_cast<DWORD>(30));
	CHECK_EQ(ring.Tail().timestamp, static_cast<DWORD>(60));
}

TEST_CASE("CRing rejects invalid logical access")
{
	CRing<TransferredData> ring(4, 4);

	CHECK_THROWS_AS(ring.Head(), CTestAssertException);
	CHECK_THROWS_AS(ring.Tail(), CTestAssertException);

	ring.AddTail(MakeTransferredData(7, 70));

	CHECK_NOTHROW(static_cast<void>(ring[0]));
	CHECK_THROWS_AS(ring[1], CTestAssertException);
}
