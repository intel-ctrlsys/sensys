#
# Copyright (c) 2015 Intel Inc. All rights reserved
#
import argparse
import os
from sqlalchemy import create_engine, DDL


def connect_to_db(db_url, verbose=False):
    """Set the db_engine instance

    :param db_url: The database connection string in the format
    postgresql[+<driver>://[<username>[:<password>]]@<server>[:<port>]/<database>.

    See SQLAlchemy database dialect for more information.
    :param verbose: Echo all SQL command
    :return:  Database engine
    """
    return create_engine(db_url, echo=verbose)


def enable_partition_trigger(db_engine, table_name, column_name, interval,
                             interval_to_keep, verbose=False):
    """Execute the generated code from generate_partition_triggers_ddl()
    function.

    :param db_engine:  The database engine to execute the SQL statements
    :param table_name: The name of the table to apply the partition
    :param column_name: The column name to partition on.  This column need to
    be a Postgres timestamp datatype
    :param interval: The interval unit supported by Postgres (e.g. YEAR, MONTH,
    DAY, HOUR, MINUTE).  The size of the partition is one unit of this interval.
    :param interval_to_keep: The number of interval to keep before purging.
    Value less than 1 will set to keep all intervals (disabled auto purging).
    :param verbose: Print the command being used
    :return:  None.  Only print output.
    """
    generated_ddl_sql_stmt = (
        """SELECT generate_partition_triggers_ddl('%s', '%s', '%s', %d);""" %
        (table_name, column_name, interval, interval_to_keep))
    if verbose:
        print(generated_ddl_sql_stmt)
    partition_trigger_ddl_stmt = db_engine.execute(
        generated_ddl_sql_stmt).scalar()

    # Enable auto purging within the table partitioning function
    if interval_to_keep > 0:
        partition_trigger_ddl_stmt = partition_trigger_ddl_stmt.replace(
            "-- EXECUTE('DROP TABLE IF EXISTS ' || quote_ident('",
            "EXECUTE('DROP TABLE IF EXISTS ' || quote_ident('")
        print("Enabled auto purging by DROP expired partition as new partition "
              "gets created")
    if verbose:
        print(partition_trigger_ddl_stmt)
    db_engine.execute(DDL(partition_trigger_ddl_stmt))

    # Verify the trigger and the partition handler functions do exist
    verify_trigger_stmt = ("SELECT 1 "
                           "FROM information_schema.triggers "
                           "WHERE trigger_name = 'insert_%s_trigger';" %
                           table_name)
    verify_trigger_result = db_engine.execute(verify_trigger_stmt).scalar()
    if verify_trigger_result:
        print("Verified 'insert_%s_trigger' exists" % table_name)
    else:
        raise RuntimeError("The expected trigger 'insert_%s_trigger' does not "
                           "exist after enabled partition on %s" %
                           (table_name, table_name))

    verify_handler_stmt = ("SELECT 1 "
                           "FROM information_schema.routines "
                           "WHERE routine_name = '%s_partition_handler';" %
                           table_name)
    verify_handler_result = db_engine.execute(verify_handler_stmt).scalar()
    if verify_handler_result:
        print("Verified '%s_partition_handler' exists" % table_name)
    else:
        raise RuntimeError("The expected partition handler function "
                           "'%s_partition_handler' does not exist after "
                           "enabled partition on %s" % (table_name, table_name))

    print("Partition trigger enabled for table '%s' on column '%s' "
          "split every 1 %s and %s" % (
              table_name, column_name, interval,
              "keep only last %d intervals" % (
                  interval_to_keep) if interval_to_keep > 0
              else "keep all intervals"))


def disable_partition_trigger(db_engine, table_name, verbose=False):
    """Drop the trigger if exists that having the name
    "insert_<table_name>_trigger" on <table_name>.

    Also drop the function if exists that having the name
    "<table_name>_partition_handler()".

     Warning:  This function is not aware of the contents of the trigger and
     function named above.  The names are the expected format generated by the
     generate_partition_triggers_ddl() function.

    :param db_engine:  The database engine to execute the SQL statements
    :param table_name: The name of the table to construct the name of the
    insert trigger and partition handler.
    :param verbose: Print the command being used
    :return:  None.  Only print output.
    """
    drop_trigger_ddl_stmt = (
        "DROP TRIGGER IF EXISTS insert_%s_trigger ON %s RESTRICT;" %
        (table_name, table_name))
    if verbose:
        print(drop_trigger_ddl_stmt)
    db_engine.execute(DDL(drop_trigger_ddl_stmt))
    drop_function_ddl_stmt = (
        "DROP FUNCTION IF EXISTS %s_partition_handler() RESTRICT;" % table_name)
    if verbose:
        print(drop_function_ddl_stmt)
    db_engine.execute(DDL(drop_function_ddl_stmt))

    print(
        "Partition trigger disabled for table '%s.'  Existing partitions left "
        "as is." % table_name)


def main():
    """Main driver"""
    main_parser = argparse.ArgumentParser(add_help=False,
        description="PostgreSQL Table Partition Helper.  Use this program to "
                    "enable table partition with and without auto dropping old "
                    "partition.  This program can also be used to disable "
                    "table partition.")
    # The parent_parser is to parse the common arguments
    parent_parser = argparse.ArgumentParser(add_help=False)
    parent_parser.add_argument("-v", "--verbose", action='store_true')
    parent_parser.add_argument("-d", "--db_url", type=str, default="",
        metavar="postgresql+driver://username:password@host:port/database",
        help="The database connection string in the format "
             "expected by SQLAlchemy.  The default value can "
             "also be set in the environment variable "
             "'PG_DB_URL'")
    parent_parser.add_argument("table", type=str,
        help="The name of the table to enable or disable partition.  This "
             "table must have at least one column that is a PostgreSQL "
             "timestamp data type.")

    subparsers = main_parser.add_subparsers(dest='subparser_name',
        help="Enable or Disable partition subcommands")
    enable_parser = subparsers.add_parser("enable", parents=[parent_parser],
                                          help="Enable partition")
    disable_parser = subparsers.add_parser("disable", parents=[parent_parser],
                                           help="Disable partition")

    enable_parser.add_argument("column", type=str,
        help="The name of the column to partition on.  This column need to be "
             "a PostgreSQL timestamp data type.")
    enable_parser.add_argument("interval", type=str,
        choices=["MINUTE", "HOUR", "DAY", "MONTH", "YEAR"],
        help="The interval unit to partition the data by.  This is the "
             "partition size.")
    enable_parser.add_argument("--interval-to-keep", type=int, default=0,
        help="The number of interval to keep before purging by.  Specify a "
             "value less than 1 will disable auto purging old partition (keep "
             "all partitions).  Default value is 0.")

    args = main_parser.parse_args()
    if args.verbose:
        print(args)

    if args.db_url:
        db_url = args.db_url
    else:
        db_url = os.getenv("PG_DB_URL")
    if not db_url:
        raise RuntimeError("Neither --db_url option is specify nor The "
                           "'PG_DB_URL' environment variable is set.  Please "
                           "use the --db_url command line option or set the "
                           "environment variable with the following pattern:\n"
                           "postgresql[+<driver>://[<username>[:<password>]]"
                           "@<server>[:<port>]/<database>\n\n"
                           "http://docs.sqlalchemy.org/en/latest/core/"
                           "engines.html#database-urls")

    db_engine = connect_to_db(db_url=db_url, verbose=args.verbose)
    if args.subparser_name == "enable":
        enable_partition_trigger(db_engine, args.table, args.column,
                                 args.interval, args.interval_to_keep,
                                 verbose=args.verbose)
    if args.subparser_name == "disable":
        disable_partition_trigger(db_engine, args.table, verbose=args.verbose)


if __name__ == "__main__":
    main()