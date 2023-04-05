#include <iostream>
#include <pqxx/pqxx>
#include "pugixml.hpp"
#include "socket.h"
using namespace std;
using namespace pqxx;
#include <fstream>
#include <string>
#include <regex>
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
                                   "status TEXT NOT NULL"
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
                        string sql_insert = "INSERT INTO account_stock (account_id, symbol_name, shares) VALUES (" + to_string(account_id) + ", '" + symbol_name + "', " + to_string(shares) + ")";
                        w.exec(sql_insert);
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
void parseXMLMessage(string msg, int msg_size, connection *C)
{
    pugi::xml_document doc;
    pugi::xml_parse_result outcome = doc.load_buffer(msg.c_str(), msg_size);
    if (!outcome)
    {
        cerr << "Error: " << outcome.description() << endl;
        exit(EXIT_FAILURE);
    }
    pugi::xml_node root = doc.first_child();
    string root_name = root.name();
    if (root_name == "create")
    {
        handleCreate(root, C);
    }
    else if (root_name == "transactions")
    {
        // Process transactions command
        // ...
    }
}

int main()
{
    connection *C = connectDB();
    int proxy_server_fd = create_server("12345");
    createTables(C);
    while (true)
    {
        // int proxy_server_fd = create_server("12345");
        char buffer[65536] = {0};
        int new_socket = accept_server(proxy_server_fd);
        int bytes_received = recv(new_socket, buffer, sizeof(buffer), 0);
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
        parseXMLMessage(msg, msg_size, C);
    }
    C->disconnect();
    return EXIT_SUCCESS;
}