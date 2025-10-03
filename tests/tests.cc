#ifndef CATCH_CONFIG_MAIN
#  define CATCH_CONFIG_MAIN
#endif

#include "atm.hpp"
#include "catch.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////
//                             Helper Definitions //
/////////////////////////////////////////////////////////////////////////////////////////////

bool CompareFiles(const std::string& p1, const std::string& p2) {
  std::ifstream f1(p1);
  std::ifstream f2(p2);

  if (f1.fail() || f2.fail()) {
    return false;  // file problem
  }

  std::string f1_read;
  std::string f2_read;
  while (f1.good() || f2.good()) {
    f1 >> f1_read;
    f2 >> f2_read;
    if (f1_read != f2_read || (f1.good() && !f2.good()) ||
        (!f1.good() && f2.good()))
      return false;
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////
// Test Cases
/////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Example: Create a new account", "[ex-1]") {
  Atm atm;
  atm.RegisterAccount(12345678, 1234, "Sam Sepiol", 300.30);
  auto accounts = atm.GetAccounts();
  REQUIRE(accounts.contains({12345678, 1234}));
  REQUIRE(accounts.size() == 1);

  Account sam_account = accounts[{12345678, 1234}];
  REQUIRE(sam_account.owner_name == "Sam Sepiol");
  REQUIRE(sam_account.balance == 300.30);

  auto transactions = atm.GetTransactions();
  REQUIRE(accounts.contains({12345678, 1234}));
  REQUIRE(accounts.size() == 1);
  std::vector<std::string> empty;
  REQUIRE(transactions[{12345678, 1234}] == empty);
}

TEST_CASE("Example: Simple widthdraw", "[ex-2]") {
  Atm atm;
  atm.RegisterAccount(12345678, 1234, "Sam Sepiol", 300.30);
  atm.WithdrawCash(12345678, 1234, 20);
  auto accounts = atm.GetAccounts();
  Account sam_account = accounts[{12345678, 1234}];

  REQUIRE(sam_account.balance == 280.30);
}

TEST_CASE("Example: Print Prompt Ledger", "[ex-3]") {
  Atm atm;
  atm.RegisterAccount(12345678, 1234, "Sam Sepiol", 300.30);
  auto& transactions = atm.GetTransactions();
  transactions[{12345678, 1234}].push_back(
      "Withdrawal - Amount: $200.40, Updated Balance: $99.90");
  transactions[{12345678, 1234}].push_back(
      "Deposit - Amount: $40000.00, Updated Balance: $40099.90");
  transactions[{12345678, 1234}].push_back(
      "Deposit - Amount: $32000.00, Updated Balance: $72099.90");
  atm.PrintLedger("./prompt.txt", 12345678, 1234);
  REQUIRE(CompareFiles("./ex-1.txt", "./prompt.txt"));
}

TEST_CASE("RegisterAccount: overwrite existing account", "[RegisterAccount][bug-1]") {
  Atm atm;
  unsigned int card = 11112222;
  unsigned int pin = 3333;
  double initial_balance = 500.00;

  REQUIRE_NOTHROW(atm.RegisterAccount(card, pin, "Alice Smith", initial_balance));

  SECTION("Attempt to overwrite with a different owner and balance (Expected: std::invalid_argument)") {
    REQUIRE_THROWS_AS(atm.RegisterAccount(card, pin, "Hacker Bob", 10000.00), std::invalid_argument);

    auto accounts = atm.GetAccounts();
    REQUIRE(accounts.contains({card, pin}));
    Account account = accounts[{card, pin}];
    REQUIRE(account.owner_name == "Alice Smith");
    REQUIRE(account.balance == initial_balance);
  }
}

TEST_CASE("WithdrawCash: Withdraw -0.0 to bypass negative check", "[WithdrawCash][bug-2]") {
  Atm atm;
  unsigned int card = 22223333;
  unsigned int pin = 4444;
  double initial_balance = 100.00;
  REQUIRE_NOTHROW(atm.RegisterAccount(card, pin, "Boundary Bob", initial_balance));

  double negative_zero = -0.0;

  SECTION("Withdraw -0.0 (Expected: std::invalid_argument)") {
    REQUIRE_THROWS_AS(atm.WithdrawCash(card, pin, negative_zero), std::invalid_argument);

    REQUIRE(atm.CheckBalance(card, pin) == initial_balance);
  }
}


TEST_CASE("DepositCash: Deposit an big amount to cause overflow", "[DepositCash][bug-3]") {
  Atm atm;
  unsigned int card = 33334444;
  unsigned int pin = 5555;
  REQUIRE_NOTHROW(atm.RegisterAccount(card, pin, "Max Deposit Mary", 1.00));

  double enormous_deposit = std::numeric_limits<double>::max() / 2.0;

  SECTION("Deposit half the maximum double value (Check for overflow to Inf)") {
    REQUIRE_NOTHROW(atm.DepositCash(card, pin, enormous_deposit));
    
    double final_balance = atm.CheckBalance(card, pin);
    REQUIRE(std::isfinite(final_balance)); 
    REQUIRE(final_balance == (1.00 + enormous_deposit));
  }
}

TEST_CASE("PrintLedger: Bad file path", "[PrintLedger][bug-4]") {
  Atm atm;
  unsigned int card = 44445555;
  unsigned int pin = 6666;
  REQUIRE_NOTHROW(atm.RegisterAccount(card, pin, "Path Traversal Patty", 10.00));
  
  atm.DepositCash(card, pin, 5.00);

  std::string malicious_path = "../../../VULNERABLE_CONFIG_FILE_LEAK.txt"; 

  SECTION("Attempt to write ledger to external path (Expected: failure/sanitization)") {
    REQUIRE_NOTHROW(atm.PrintLedger(malicious_path, card, pin));
    
    std::ifstream malicious_file(malicious_path);
    REQUIRE(malicious_file.good()); 
    
    remove(malicious_path.c_str());
  }
}