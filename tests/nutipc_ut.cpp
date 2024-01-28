/*
    NUT IPC unit test

        Copyright (C) 2012

            \author Vaclav Krpec <VaclavKrpec@Eaton.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "config.h"

#include "nutipc.hpp"
#include "nutstream.hpp"

#include <cppunit/extensions/HelperMacros.h>

#include <sstream>

extern "C" {
#include <unistd.h>
#include <signal.h>
#include <string.h>
}


/**
 *  \brief  NUT IPC module unit test
 */
class NutIPCUnitTest: public CppUnit::TestFixture {
	private:

	CPPUNIT_TEST_SUITE(NutIPCUnitTest);
		CPPUNIT_TEST(test);
	CPPUNIT_TEST_SUITE_END();

	/** External command execution test */
	void testExec();

	/**
	 *  \brief  Test signal handler
	 *
	 *  \param  signal  Signal caught
	 */
	static void testSignalHandler(int signal);

	/** Signal sending test */
	void testSignalSend();

	/** Signal receiving test */
	void testSignalRecv();

	public:

	inline void setUp() override {}
	inline void tearDown() override {}

	inline void test() {
		testExec();
		testSignalSend();
		testSignalRecv();
	}

	virtual ~NutIPCUnitTest() override;
};  // end of class NutIPCUnitTest

// Register the test suite
CPPUNIT_TEST_SUITE_REGISTRATION(NutIPCUnitTest);


void NutIPCUnitTest::testExec() {
	static const std::string bin = "/bin/sh";

	nut::Process::Executor::Arguments args;

	args.push_back("-c");
	args.push_back("exit 123");

	nut::Process::Execution child(bin, args);

	CPPUNIT_ASSERT(123 == child.wait());

	CPPUNIT_ASSERT(0 == nut::Process::execute("test 'Hello world' == 'Hello world'"));
}


/** Last signal caught */
static int signal_caught = 0;

void NutIPCUnitTest::testSignalHandler(int signal) {
	signal_caught = signal;
}

void NutIPCUnitTest::testSignalSend() {
	struct sigaction action;

	pid_t my_pid = nut::Process::getPID();

	// Set SIGUSR1 signal handler
	::memset(&action, 0, sizeof(action));
	::sigemptyset(&action.sa_mask);

	action.sa_handler = &testSignalHandler;

	CPPUNIT_ASSERT(0 == ::sigaction(static_cast<int>(nut::Signal::USER1), &action, nullptr));

	// Send signal directly
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));

	CPPUNIT_ASSERT(static_cast<int>(nut::Signal::USER1) == signal_caught);

	signal_caught = 0;

	// Save PID to a PIDfile
	static const std::string pid_file_name("/tmp/foobar.pid");

	std::stringstream my_pid_ss;

	my_pid_ss << my_pid;

	nut::NutFile pid_file(pid_file_name, nut::NutFile::WRITE_ONLY);

	pid_file.putString(my_pid_ss.str());

	pid_file.closex();

	// Send signal to process via the PIDfile
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, pid_file_name));

	CPPUNIT_ASSERT(static_cast<int>(nut::Signal::USER1) == signal_caught);

	pid_file.removex();

	signal_caught = 0;
}


/** Caught signal list */
static nut::Signal::List caught_signals;

/** Signal handler routine */
class TestSignalHandler: public nut::Signal::Handler {
	public:

	void operator () (nut::Signal::enum_t signal) override {
		caught_signals.push_back(signal);
	}

	virtual ~TestSignalHandler() override;
};  // end of class TestSignalHandler

void NutIPCUnitTest::testSignalRecv() {
	// Create signal handler thread
	nut::Signal::List signals;

	signals.push_back(nut::Signal::USER1);
	signals.push_back(nut::Signal::USER2);

	nut::Signal::HandlerThread<TestSignalHandler> sig_handler(signals);

	pid_t my_pid = nut::Process::getPID();

	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER2, my_pid));
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));
	CPPUNIT_ASSERT(0 == nut::Signal::send(nut::Signal::USER1, my_pid));

	// Let the sig. handler thread finish...
	::sleep(1);

	CPPUNIT_ASSERT(caught_signals.size() == 3);

	CPPUNIT_ASSERT(caught_signals.front() == nut::Signal::USER2);

	caught_signals.pop_front();

	CPPUNIT_ASSERT(caught_signals.front() == nut::Signal::USER1);

	caught_signals.pop_front();

	CPPUNIT_ASSERT(caught_signals.front() == nut::Signal::USER1);
}

// Implement out of class declaration to avoid
//   error: 'SomeClass' has no out-of-line virtual method
//   definitions; its vtable will be emitted in every translation unit
//   [-Werror,-Wweak-vtables]
TestSignalHandler::~TestSignalHandler() {}
NutIPCUnitTest::~NutIPCUnitTest() {}
