#include <iostream>
#include <pqxx/pqxx>
#include "pugixml.hpp"
#include <ctime>
#include <iomanip>
#include "socket.h"
using namespace std;
using namespace pqxx;
#include <fstream>
#include <string>
#include <sstream>
#include <pthread.h>
#include <regex>
#include <atomic>

std::atomic<int> trans_id(0); // global variable
connection *connectDB()
{
    // Allocate & initialize a Postgres connection object
    connection *C;
    try
    {
        // Establish a connection to the database
        // Parameters: database name, user name, user password
        C = new connection("dbname=TRADER user=postgres password=passw0rd");
        if (C->is_open())
        {
            cout << "Opened database successfully: " << C->dbname() << endl;
        }
        else
        {
            cout << "Can't open database" << endl;
            exit(EXIT_FAILURE);
            // return 1;
        }
    }
    catch (const std::exception &e)
    {
        cerr << e.what() << std::endl;
        exit(EXIT_FAILURE);
        // return 1;
    }

    return C;
    // Close database connection
}

void createTables(connection *C)
{
    try
    {
        work W(*C);
        // The order to drop the tables should be in the reverse order of their dependencies.

        W.exec("DROP TABLE IF EXISTS account_stock;");
        W.exec("DROP TABLE IF EXISTS orders;");
        W.exec("DROP TABLE IF EXISTS account;");
        // W.exec("DROP TABLE IF EXISTS transactions;");

        string createAccountTable = "CREATE TABLE account ("
                                    "id INTEGER PRIMARY KEY,"
                                    "balance NUMERIC(15,2) NOT NULL CHECK (balance >= 0)"
                                    ");";

        string createAccountStockTable = "CREATE TABLE account_stock ("
                                         "account_id INTEGER NOT NULL REFERENCES account(id),"
                                         "symbol_name TEXT NOT NULL," // variable-length strings of text.
                                         "shares INTEGER NOT NULL,"
                                         "PRIMARY KEY (account_id, symbol_name)"
                                         ");";

        string createOrdersTable = "CREATE TABLE orders ("
                                   "id SERIAL PRIMARY KEY NOT NULL,"
                                   "trans_id INTEGER NOT NULL," // need to be assigned ourself
                                   "account_id INTEGER NOT NULL REFERENCES account(id),"
                                   "symbol_name TEXT NOT NULL,"
                                   "amount INTEGER NOT NULL,"
                                   "price NUMERIC(8, 2)NOT NULL,"
                                   "side TEXT NOT NULL,"
                                   "status TEXT NOT NULL,"
                                   "time TIMESTAMP DEFAULT NOW() NOT NULL"
                                   ");";

        W.exec(createAccountTable);
        W.exec(createAccountStockTable);
        W.exec(createOrdersTable);
        // W.exec(createTransactionsTable);
        W.commit();
    }
    catch (const exception &e)
    {
        cerr << e.what() << endl;
        exit(EXIT_FAILURE);
    }
}
pugi::xml_document handleCreate(pugi::xml_node xml_root, connection *C)
{
    pugi::xml_document response;
    pugi::xml_node res_root = response.append_child("results");
    for (pugi::xml_node node : xml_root.children())
    {
        string node_name = node.name();
        work w(*C);
        if (node_name == "account")
        {
            int account_id = node.attribute("id").as_int();
            double balance = node.attribute("balance").as_double();

            // Insert account into account table
            // work w(*C);
            string sql_check_id = "SELECT COUNT(*) FROM account WHERE id = " + to_string(account_id);
            result R_check(w.exec(sql_check_id)); // do not use the variable result !!!!!(namespace)
            if (R_check[0][0].as<int>() > 0)      // Account already exists, return error response
            {
                pugi::xml_node error_node = res_root.append_child("error");
                error_node.append_attribute("id") = to_string(account_id).c_str();
                error_node.append_child(pugi::node_pcdata).set_value("Account creation failed: Account already exists.");
            }
            else
            {
                if (balance < 0)
                {
                    pugi::xml_node error_node = res_root.append_child("error");
                    error_node.append_attribute("id") = to_string(account_id).c_str();
                    error_node.append_child(pugi::node_pcdata).set_value("Account creation failed: Balance is not positive.");
                }
                else
                {
                    string sql = "INSERT INTO account (id, balance) VALUES (" + to_string(account_id) + ", " + to_string(balance) + ")";
                    w.exec(sql);
                    pugi::xml_node created_node = res_root.append_child("created");
                    created_node.append_attribute("id") = to_string(account_id).c_str();
                }
            }

            // w.exec(sql);
            w.commit();
        }
        else if (node_name == "symbol")
        {
            string symbol_name = node.attribute("sym").as_string();
            regex alnum_pattern("[[:alnum:]]+");

            if (!regex_match(symbol_name, alnum_pattern))
            {
                // pugi::xml_node error_node = res_root.append_child("error");
                // error_node.append_attribute("sym") = symbol_name.c_str();
                // error_node.append_child(pugi::node_pcdata).set_value("Symbol creation failed: Symbol name must be alphanumeric."); // plain character data
                for (pugi::xml_node account_node : node.children("account"))
                {
                    int account_id = account_node.attribute("id").as_int();
                    pugi::xml_node account_error_node = res_root.append_child("error");
                    account_error_node.append_attribute("sym") = symbol_name.c_str();
                    account_error_node.append_attribute("id") = to_string(account_id).c_str();
                    account_error_node.append_child(pugi::node_pcdata).set_value("Account creation failed: Invalid symbol name.");
                }
            }
            else
            {
                for (pugi::xml_node account_node : node.children("account"))
                {
                    int account_id = account_node.attribute("id").as_int();
                    int shares = account_node.text().as_int();
                    // Check if the account exists
                    // work w(*C);
                    string sql_check = "SELECT COUNT(*) FROM account WHERE id = " + to_string(account_id);
                    result R_check(w.exec(sql_check));
                    int count = R_check[0][0].as<int>();
                    if (count == 0) // if not exist, we cannot put stock in it
                    {
                        pugi::xml_node error_node = res_root.append_child("error");
                        error_node.append_attribute("sym") = symbol_name.c_str();
                        error_node.append_attribute("id") = to_string(account_id).c_str();
                        error_node.append_child(pugi::node_pcdata).set_value("Account not found.");
                    }
                    else
                    {
                        string sql_insert = "INSERT INTO account_stock (account_id, symbol_name, shares) VALUES (" + to_string(account_id) + ", '" + symbol_name + "', " + to_string(shares) + ") ON CONFLICT (account_id, symbol_name) DO UPDATE SET shares = account_stock.shares + EXCLUDED.shares RETURNING *";
                        result R_insert(w.exec(sql_insert));

                        pugi::xml_node created_node = res_root.append_child("created");
                        created_node.append_attribute("sym") = symbol_name.c_str();
                        created_node.append_attribute("id") = to_string(account_id).c_str();
                    }
                    // Insert account_stock into account_stock table
                    // work w(*C);
                    // w.exec(sql);
                }
            }
            w.commit();
        }
    }
    return response;
}

void errorTransOrder(pugi::xml_node &error_node, const string &symbol_name, int amount, double price)
{
    error_node.append_attribute("sym") = symbol_name.c_str();
    error_node.append_attribute("amount") = to_string(amount).c_str();
    error_node.append_attribute("limit") = to_string(price).c_str();
}
// child_node is the original data, and the res_root is the place where we want to write
void handleTransOrder(const pugi::xml_node &child_node, int account_id, connection *C, pugi::xml_node &res_root, int &new_order_id)
{
    string symbol_name = child_node.attribute("sym").as_string();
    int amount = child_node.attribute("amount").as_int();
    double price = child_node.attribute("limit").as_double();

    // Check if the symbol_name is alphanumeric
    // cannot abstract, since it is not the same with other error format
    regex alnum_pattern("[[:alnum:]]+");
    if (!regex_match(symbol_name, alnum_pattern))
    {
        pugi::xml_node error_node = res_root.append_child("error");
        errorTransOrder(error_node, symbol_name, amount, price);
        error_node.append_child(pugi::node_pcdata).set_value("Symbol name must be alphanumeric.");
        return;
    }

    // Check if the price is positive
    if (price <= 0)
    {
        pugi::xml_node error_node = res_root.append_child("error");
        errorTransOrder(error_node, symbol_name, amount, price);
        error_node.append_child(pugi::node_pcdata).set_value("Price must be positive.");
        return;
    }

    work w(*C);
    // Check if the account exists
    string sql_check_account = "SELECT balance FROM account WHERE id = " + to_string(account_id);
    result R_check_account(w.exec(sql_check_account));
    double balance = R_check_account[0][0].as<double>();
    if (amount > 0) // Buy operation
    {
        if (balance >= amount * price)
        {
            // Reduce the amount * price in the account
            string sql_update_account = "UPDATE account SET balance = balance - " + to_string(amount * price) + " WHERE id = " + to_string(account_id);
            w.exec(sql_update_account);
        }
        else
        {
            pugi::xml_node error_node = res_root.append_child("error");
            errorTransOrder(error_node, symbol_name, amount, price);
            error_node.append_child(pugi::node_pcdata).set_value("Insufficient funds.");
            return;
        }
    }
    else if (amount < 0) // Sell operation
    {
        // Check if the shares of the account >= amount
        string sql_check_shares = "SELECT shares FROM account_stock WHERE account_id = " + to_string(account_id) + " AND symbol_name = '" + symbol_name + "'";
        result R_check_shares(w.exec(sql_check_shares));

        if (!R_check_shares.empty() && R_check_shares[0][0].as<int>() >= -amount)
        {
            // Reduce the shares in the account
            string sql_update_shares = "UPDATE account_stock SET shares = shares + " + to_string(amount) + " WHERE account_id = " + to_string(account_id) + " AND symbol_name = '" + symbol_name + "'";
            w.exec(sql_update_shares);
        }
        else
        {
            pugi::xml_node error_node = res_root.append_child("error");
            errorTransOrder(error_node, symbol_name, amount, price);
            error_node.append_child(pugi::node_pcdata).set_value("Insufficient shares.");
            return;
        }
    }
    else
    {
        pugi::xml_node error_node = res_root.append_child("error");
        errorTransOrder(error_node, symbol_name, amount, price);
        error_node.append_child(pugi::node_pcdata).set_value("Amount cannot be zero.");
        return;
    }

    // If everything is successful, append the appropriate response
    pugi::xml_node opened_node = res_root.append_child("opened");
    opened_node.append_attribute("sym") = symbol_name.c_str();
    opened_node.append_attribute("amount") = to_string(amount).c_str();
    opened_node.append_attribute("limit") = to_string(price).c_str();
    opened_node.append_attribute("id") = to_string(account_id).c_str();

    string side = (amount < 0) ? "sell" : "buy";
    string sql_insert_order = "INSERT INTO orders (trans_id, account_id, symbol_name, amount, price, side, status) VALUES (" + to_string(trans_id.fetch_add(1)) + ", " + to_string(account_id) + ", '" + symbol_name + "', " + to_string(amount) + ", " + to_string(price) + ", '" + side + "', 'open') RETURNING id";
    result R_insert_order(w.exec(sql_insert_order));
    new_order_id = R_insert_order[0][0].as<int>();
    w.commit();
}
void orderMatch(int new_order_id, connection *C)
{
    // every SQL command execute is immediately committed to the database, and there's no need to call commit() explicitly
    work w(*C);
    // Get the new order details
    string sql_get_order = "SELECT * FROM orders WHERE id = " + to_string(new_order_id);
    const pqxx::result &R_new_order = w.exec(sql_get_order);
    w.commit();
    string new_side = R_new_order[0]["side"].as<string>();
    int new_amount = R_new_order[0]["amount"].as<int>();
    double new_price = R_new_order[0]["price"].as<double>();
    string opposite_side = new_side == "buy" ? "sell" : "buy";
    string price_condition = new_side == "buy" ? "<=" : ">=";

    string symbol_name = R_new_order[0]["symbol_name"].as<string>();
    // Continue matching while new_amount is not zero
    while (new_amount != 0)
    {
        // Find a matching order
        work w1(*C);
        string sql_find_matching_order = "SELECT * FROM orders WHERE side = '" + opposite_side + "' AND status = 'open' AND id < " + to_string(new_order_id) + " AND price " + price_condition + " " + to_string(new_price) + " ORDER BY price " + (new_side == "buy" ? "ASC" : "DESC") + ", id ASC LIMIT 1";
        const pqxx::result &R_matching_order = w1.exec(sql_find_matching_order);

        // No matching order found, break the loop
        if (R_matching_order.empty())
        {
            w1.commit();
            break;
        }

        int matching_order_id = R_matching_order[0]["id"].as<int>();
        int matching_amount = R_matching_order[0]["amount"].as<int>();
        double matching_price = R_matching_order[0]["price"].as<double>();

        // Calculate the executed amount and the remaining amounts for both orders
        int executed_amount = min(abs(new_amount), abs(matching_amount));
        int new_remaining_amount = new_amount - (new_side == "buy" ? executed_amount : -executed_amount);
        int matching_remaining_amount = matching_amount - (opposite_side == "buy" ? executed_amount : -executed_amount);

        // Determine the buyer's and seller's account IDs
        int buyer_account_id = new_side == "buy" ? R_new_order[0]["account_id"].as<int>() : R_matching_order[0]["account_id"].as<int>();
        int seller_account_id = new_side == "buy" ? R_matching_order[0]["account_id"].as<int>() : R_new_order[0]["account_id"].as<int>();

        // Update the new order amount
        new_amount = new_remaining_amount;

        // Update the matching order
        if (matching_remaining_amount == 0)
        {
            // If matching order is fully executed, update its status and price
            // work w1(*C);
            string sql_update_matching_order = "UPDATE orders SET status = 'executed', price = " + to_string(matching_price) + ", time = now() WHERE id = " + to_string(matching_order_id);
            w1.exec(sql_update_matching_order);
        }
        else
        {
            // If matching order is partially executed, update its amount and status, and create a new executed order

            string sql_update_matching_order = "UPDATE orders SET amount = " + to_string(matching_remaining_amount) + " WHERE id = " + to_string(matching_order_id);
            w1.exec(sql_update_matching_order);

            string sql_create_executed_order = "INSERT INTO orders (trans_id, account_id, symbol_name, amount, price, side, status) VALUES (" + to_string(R_matching_order[0]["trans_id"].as<int>()) + ", " + to_string(R_matching_order[0]["account_id"].as<int>()) + ", '" + R_matching_order[0]["symbol_name"].as<string>() + "', " + to_string(opposite_side == "buy" ? executed_amount : -executed_amount) + ", " + to_string(matching_price) + ", '" + opposite_side + "', 'executed')";
            w1.exec(sql_create_executed_order);
        }

        if (new_remaining_amount == 0)
        {
            // If the new order is fully executed, update its status
            string sql_update_new_order = "UPDATE orders SET status = 'executed', price = " + to_string(matching_price) + ",time = now() WHERE id = " + to_string(new_order_id);
            w1.exec(sql_update_new_order);
        }
        else
        {

            string sql_update_new_order = "UPDATE orders SET amount = " + to_string(new_remaining_amount) + ",time = now() WHERE id = " + to_string(new_order_id);
            w1.exec(sql_update_new_order);
            // If the new order is partially executed, create a new executed order with the executed amount and matching price
            string sql_create_executed_order = "INSERT INTO orders (trans_id, account_id, symbol_name, amount, price, side, status) VALUES (" + to_string(R_new_order[0]["trans_id"].as<int>()) + ", " + to_string(R_new_order[0]["account_id"].as<int>()) + ", '" + R_new_order[0]["symbol_name"].as<string>() + "', " + to_string(new_side == "buy" ? executed_amount : -executed_amount) + ", " + to_string(matching_price) + ", '" + new_side + "', 'executed')";
            w1.exec(sql_create_executed_order);
        }
        // Update the buyer's account in the account_stock table

        string sql_update_buyer_stock = "INSERT INTO account_stock (account_id, symbol_name, shares) VALUES (" + to_string(buyer_account_id) + ", '" + symbol_name + "', " + to_string(executed_amount) + ") ON CONFLICT (account_id, symbol_name) DO UPDATE SET shares = account_stock.shares + EXCLUDED.shares";
        w1.exec(sql_update_buyer_stock);

        // Update the seller's balance in the account table
        if (new_side == "buy")
        {
            string diff_buyer = "UPDATE account SET balance = balance + " + to_string(executed_amount * (new_price - matching_price)) + " WHERE id = " + to_string(buyer_account_id);
            w1.exec(diff_buyer);
        }

        string sql_update_seller_balance = "UPDATE account SET balance = balance + " + to_string(executed_amount * matching_price) + " WHERE id = " + to_string(seller_account_id);
        w1.exec(sql_update_seller_balance);
        w1.commit();
    }
    /*
    if (new_amount > 0)
    {
        string sql_update_new_order = "UPDATE orders SET amount = " + to_string(new_amount) + ",time = now() WHERE id = " + to_string(new_order_id);
        w.exec(sql_update_new_order);
        w.commit();
    }
    */
    // w.commit();
}
// child_node is the original data, res_root is the results
void handleQueryOrder(pugi::xml_node child_node, pqxx::connection *C, pugi::xml_node &res_root)
{
    int order_id = child_node.attribute("id").as_int();
    work w(*C);

    // Get the trans_id from the order with the given id
    std::string sql_get_trans_id = "SELECT trans_id FROM orders WHERE id = " + std::to_string(order_id);
    pqxx::result R_trans_id = w.exec(sql_get_trans_id);

    if (!R_trans_id.empty())
    {
        int trans_id = R_trans_id[0][0].as<int>();

        // Find all orders with the same trans_id
        std::string sql_get_orders = "SELECT * FROM orders WHERE trans_id = " + std::to_string(trans_id) + " ORDER BY id";
        pqxx::result R_orders = w.exec(sql_get_orders);

        pugi::xml_node status_node = res_root.append_child("status");
        status_node.append_attribute("id") = trans_id;

        for (const auto &row : R_orders)
        {
            std::string status = row["status"].as<std::string>();

            pugi::xml_node order_node = status_node.append_child(status.c_str());
            order_node.append_attribute("shares") = row["amount"].as<int>();
            if (status == "executed")
            {
                order_node.append_attribute("price") = row["price"].as<double>();
            }
            if (status == "canceled" || status == "executed")
            {
                std::string time = row["time"].as<std::string>();
                std::tm tm = {};
                std::istringstream ss(time);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                std::time_t timestamp = std::mktime(&tm);
                order_node.append_attribute("time") = timestamp;
            }
        }
    }
}
void handleCancelOrder(pugi::xml_node child_node, pqxx::connection *C, pugi::xml_node &res_root)
{
    int order_id = child_node.attribute("id").as_int();
    work w(*C);
    std::string sql_get_trans_id = "SELECT trans_id FROM orders WHERE id = " + std::to_string(order_id);
    pqxx::result R_trans_id = w.exec(sql_get_trans_id);
    if (!R_trans_id.empty())
    {
        int trans_id = R_trans_id[0][0].as<int>();
        pugi::xml_node canceled_node = res_root.append_child("canceled");
        canceled_node.append_attribute("id") = trans_id;

        // Find all orders with the same trans_id
        std::string sql_get_orders = "SELECT * FROM orders WHERE trans_id = " + std::to_string(trans_id) + " ORDER BY id";
        pqxx::result R_orders = w.exec(sql_get_orders);

        for (const auto &row : R_orders)
        {
            int current_order_id = row["id"].as<int>();
            std::string status = row["status"].as<std::string>();
            int shares = row["amount"].as<int>();
            double price = row["price"].as<double>();
            std::string time = row["time"].as<std::string>();
            string symbol_name = row["symbol_name"].as<string>();

            int account_id = row["account_id"].as<int>();
            if (status == "open")
            {
                // work w1(*C);
                //  Update the status to "canceled" and set the time to now
                std::string sql_update_order = "UPDATE orders SET status = 'canceled', time = now() WHERE id = " + std::to_string(current_order_id);
                w.exec(sql_update_order);
                // w1.commit();
                //  Append the canceled shares and time to the response
                pugi::xml_node canceled_shares_node = canceled_node.append_child("canceled");
                canceled_shares_node.append_attribute("shares") = shares;
                std::tm tm = {};
                std::istringstream ss(time);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                std::time_t timestamp = std::mktime(&tm);

                canceled_shares_node.append_attribute("time") = timestamp;
                // If it's a buy order, return the amount * price to the account's balance
                if (shares > 0)
                {
                    double order_cost = shares * price;
                    std::string sql_update_balance = "UPDATE account SET balance = balance + " + std::to_string(order_cost) + " WHERE id = " + std::to_string(account_id);
                    w.exec(sql_update_balance);
                }
                // If it's a sell order, return the shares to the account's stock holdings
                else if (shares < 0)
                {
                    std::string sql_update_shares = "INSERT INTO account_stock (account_id, symbol_name, shares) VALUES (" + std::to_string(account_id) + ", '" + symbol_name + "', " + std::to_string(shares) + ") ON CONFLICT (account_id, symbol_name) DO UPDATE SET shares = account_stock.shares - EXCLUDED.shares";
                    w.exec(sql_update_shares);
                }
            }
            else if (status == "executed")
            {
                // Append the executed shares, price, and time to the response
                pugi::xml_node executed_shares_node = canceled_node.append_child("executed");
                executed_shares_node.append_attribute("shares") = shares;
                executed_shares_node.append_attribute("price") = price;
                std::tm tm = {};
                std::istringstream ss(time);
                ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
                std::time_t timestamp = std::mktime(&tm);
                executed_shares_node.append_attribute("time") = timestamp;
            }
        }

        w.commit();
    }
}
// xml_root is the original data
pugi::xml_document handleTrans(pugi::xml_node xml_root, connection *C)
{
    pugi::xml_document res_doc;
    pugi::xml_node res_root = res_doc.append_child("results");

    int account_id = xml_root.attribute("id").as_int();

    // Check if the account exists
    work w(*C);
    string sql_check = "SELECT COUNT(*) FROM account WHERE id = " + to_string(account_id);
    result R_check(w.exec(sql_check));
    w.commit();
    int count = R_check[0][0].as<int>();

    for (pugi::xml_node child_node : xml_root.children())
    {
        string node_name = child_node.name();

        if (node_name == "order")
        {
            // Process order command
            // ...
            string symbol_name = child_node.attribute("sym").as_string();
            int amount = child_node.attribute("amount").as_int();
            double price = child_node.attribute("limit").as_double();
            if (count == 0)
            {
                pugi::xml_node error_node = res_root.append_child("error");
                error_node.append_attribute("sym") = symbol_name.c_str();
                error_node.append_attribute("amount") = to_string(amount).c_str();
                error_node.append_attribute("limit") = to_string(price).c_str();
                error_node.append_child(pugi::node_pcdata).set_value("Account not found.");
                continue;
            }
            else
            {
                int new_order_id = -1;
                handleTransOrder(child_node, account_id, C, res_root, new_order_id);
                if (new_order_id == -1)
                {
                    continue;
                }
                else
                {
                    orderMatch(new_order_id, C);
                }
            }
        }
        else if (node_name == "query")
        {
            if (count == 0)
            {
                // int order_id = child_node.attribute("id").as_int();
                pugi::xml_node error_node = res_root.append_child("error");
                error_node.append_attribute("id") = to_string(account_id).c_str();
                error_node.append_child(pugi::node_pcdata).set_value("Account not found.");
                continue;
            }
            else
            {
                // if this order does not exist, it just show the empty results, not returning errors
                handleQueryOrder(child_node, C, res_root);
            }
            // Process query command
            // ...
        }
        else if (node_name == "cancel")
        {
            if (count == 0)
            {
                pugi::xml_node error_node = res_root.append_child("error");
                error_node.append_attribute("id") = to_string(account_id).c_str();
                error_node.append_child(pugi::node_pcdata).set_value("Account not found.");
                continue;
            }
            else
            {
                handleCancelOrder(child_node, C, res_root);
            }
            // Process cancel command
            // ...
        }
    }
    return res_doc;
}

string parseXMLMessage(string msg, int msg_size, connection *C)
{
    pugi::xml_document doc;
    pugi::xml_parse_result outcome = doc.load_buffer(msg.c_str(), msg_size);
    if (!outcome)
    {
        cerr << "Error: " << outcome.description() << endl;
        exit(EXIT_FAILURE);
    }
    pugi::xml_node root = doc.first_child(); // in transaction or create
    string root_name = root.name();
    pugi::xml_document response;
    if (root_name == "create")
    {
        response = handleCreate(root, C);
    }
    else if (root_name == "transactions")
    {
        response = handleTrans(root, C);
        // Process transactions command
        // ...
    }
    stringstream ss;
    response.save(ss);
    string response_str = ss.str();
    return response_str;
}
class threadInfo
{
public:
    int new_socket;
};

void *threadParseXML(void *arg)
{
    // threadInfo *thr_arg = (threadInfo *)arg;
    int new_socket = *(int *)arg;
    connection *C = connectDB();

    char buffer[65536] = {0};
    int bytes_received = recv(new_socket, buffer, sizeof(buffer), 0);
    cout << "server receive: " << buffer << endl;
    if (bytes_received < 0)
    {
        cerr << "Error: cannot receive message" << endl;
        exit(EXIT_FAILURE);
    }

    // Convert the received buffer to a string
    string msg(buffer, bytes_received);

    // Parse the message size from the first line
    int size_end = msg.find('\n');

    if (size_end == string::npos)
    {
        cout << "error msg:" << msg << "...." << endl;
        cerr << "Error: missing newline character" << endl;
        exit(EXIT_FAILURE);
    }
    string size_str = msg.substr(0, size_end);
    msg = msg.substr(size_end + 1);
    int msg_size = stoi(size_str);

    // Check that the message size is correct
    if (msg.size() != msg_size)
    {
        cerr << "Error: message size does not match" << endl;
        exit(EXIT_FAILURE);
    }
    string response = parseXMLMessage(msg, msg_size, C);
    send(new_socket, response.c_str(), response.length(), 0);
    C->disconnect();
    delete C;
    close(new_socket);
    delete (int *)arg;
    return NULL;
}

int main()
{
    connection *C = connectDB();
    int proxy_server_fd = create_server("12345");
    createTables(C);
    C->disconnect();
    while (true)
    {
        // int proxy_server_fd = create_server("12345");

        int new_socket = accept_server(proxy_server_fd);
        // cout << "socket used: " << new_socket << endl;
        pthread_t thread;
        // threadInfo *infor = new threadInfo();
        //  infor->c = C;
        // infor->new_socket = new_socket;
        int *socket_ptr = new int(new_socket);
        pthread_create(&thread, NULL, threadParseXML, socket_ptr);
        pthread_detach(thread);
        // close(new_socket);
    }

    return EXIT_SUCCESS;
}