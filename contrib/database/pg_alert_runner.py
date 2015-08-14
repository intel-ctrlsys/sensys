#
# Copyright (c) 2015 Intel Inc. All rights reserved
#
import argparse
import os
from sqlalchemy import create_engine, exc
import time
import datetime


def connect_to_db(db_url, verbose=False):
    """Set the db_engine instance

    :param db_url: The database connection string in the format
    postgresql[+<driver>://[<username>[:<password>]]@<server>[:<port>]/<database>.d

    See SQLAlchemy database dialect for more information.
    :param verbose: Echo all SQL command
    :return:  Database engine
    """
    return create_engine(db_url, echo=verbose)


def verify_stored_procedure_exists(db_engine, stored_procedure_name):
    """Verify a given stored procedure exists.  Raise a runtime exception if it
    does not exist.

    :param db_url: The database connection string in the format
    postgresql[+<driver>://[<username>[:<password>]]@<server>[:<port>]/<database>.d

    See SQLAlchemy database dialect for more information.
    :param stored_procedure_name:  The name of the stored procedure to check
    for its existence in the current database.
    """
    verify_sp_exists_stmt = ("SELECT 1 "
                             "FROM information_schema.routines "
                             "WHERE routine_name = '%s' "
                             "    AND routine_catalog = current_database();" %
                             stored_procedure_name)
    verify_sp_exists_result = db_engine.execute(verify_sp_exists_stmt).scalar()
    if verify_sp_exists_result != 1:
        raise RuntimeError("The expected stored procedure '%s' "
                           "does not exist.  Check the the database schema "
                           "version." % stored_procedure_name)

def run_alert_db_size(db_engine, max_db_size_in_gb, log_level_percentage,
                      warning_level_percentage, critical_level_percentage,
                      log_tag, print_status, verbose=False):
    """Wrapper to run the 'alert_db_size' stored procedure

    :param db_url: The database connection string in the format
    postgresql[+<driver>://[<username>[:<password>]]@<server>[:<port>]/<database>.d

    See SQLAlchemy database dialect for more information.
    :param max_db_size_in_gb:  The maximum database size expressed in Gigabytes
    to be considered as 100% full.  Value must be greater than 0.
    :param log_level_percentage:  The percentage of the maximum database
    size to trigger a 'LOG' level.  Value must be between 0.0 and 1.0.
    :param warning_level_percentage:  The percentage of the maximum database
    size to trigger a 'WARNING' level.  Value must be between 0.0 and 1.0.
    :param critical_level_percentage:  The percentage of the maximum database
    size to trigger a 'CRITICAL' level.  Value must be between 0.0 and 1.0.
    :param log_tag:  The string to be included in the log message.
    :param print_status: True to print the result status from the function call.
    :param verbose: Print all messages include the database command being used
    :return: None
    """
    select_alert_db_size_sql_stmt = (
        """SELECT alert_db_size(%f, %f, %f, %f, '%s');""" %
        (max_db_size_in_gb, log_level_percentage, warning_level_percentage,
         critical_level_percentage, log_tag))
    if verbose:
        print(select_alert_db_size_sql_stmt)
    try:
        status = db_engine.execute(select_alert_db_size_sql_stmt).scalar()
    except exc.InternalError as e:
        status = str(e)
    if print_status or verbose:
        print("%s %s" % (datetime.datetime.now(), status))


def main():
    """Main driver"""
    main_parser = argparse.ArgumentParser(
        description="PostgreSQL Database Alert Runner.  Use this program to "
                    "run the DB Alert monitor.")
    main_parser.add_argument("-v", "--verbose", action='store_true',
        help="Print all messages include the database command being used")
    main_parser.add_argument("-p", "--hide-status", action='store_false',
        help="Hide the output (not to print) from each run of the Database "
             "Alert.  The check stills log the output in the PostgreSQL "
             "log configured by PostgreSQL's 'log_destination'.")
    main_parser.add_argument("-d", "--db_url", type=str, default="",
        metavar="postgresql+driver://username:password@host:port/database",
        help="The database connection string in the format "
             "expected by SQLAlchemy.  The default value can "
             "also be set in the environment variable "
             "'PG_DB_URL'")
    main_parser.add_argument("max_db_size_in_gb", type=float,
        help="The maximum database size expressed in Gigabytes to be "
             "considered as 100%% full.  Value must be greater than 0.")

    main_parser.add_argument("--log-level-percentage", type=float,
        default=0.50,
        help="The percentage of the maximum database size to trigger a 'LOG' "
             "level.  Value must be between 0.0 and 1.0.")
    main_parser.add_argument("--warning-level-percentage", type=float,
        default=0.80,
        help="The percentage of the maximum database size to trigger a "
             "'WARNING' level.  Value must be between 0.0 and 1.0.")
    main_parser.add_argument("--critical-level-percentage", type=float,
        default=0.90,
        help="The percentage of the maximum database size to trigger a "
             "'CRITICAL' level.  Value must be between 0.0 and 1.0.")
    main_parser.add_argument("--log-tag", type=str,
        default="ORCMDB Alert",
        help="The string to be included in the log message.")

    main_parser.add_argument("-l", "--loop", type=int,
        default=0,
        help="The number of seconds between each endless loop.  "
             "Value less than 1 will result in a single execution.")

    args = main_parser.parse_args()
    if args.verbose:
        print(args)

    if args.db_url:
        db_url = args.db_url
    else:
        db_url = os.getenv("PG_DB_URL")
    if not db_url:
        raise RuntimeError("Neither --db_url option is specified nor The "
                           "'PG_DB_URL' environment variable is set.  Please "
                           "use the --db_url command-line option or set the "
                           "environment variable with the following pattern:\n"
                           "postgresql[+<driver>://[<username>[:<password>]]"
                           "@<server>[:<port>]/<database>\n\n"
                           "http://docs.sqlalchemy.org/en/latest/core/"
                           "engines.html#database-urls")

    db_engine = connect_to_db(db_url=db_url, verbose=args.verbose)
    verify_stored_procedure_exists(db_engine, 'alert_db_size')
    if args.loop > 0:
        print("Start calling alert database size every %d %s" %
              (args.loop, "second" if args.loop == 1 else "seconds"))
        while True:
            run_alert_db_size(db_engine, args.max_db_size_in_gb,
                              args.log_level_percentage,
                              args.warning_level_percentage,
                              args.critical_level_percentage,
                              args.log_tag,
                              print_status=args.hide_status,
                              verbose=args.verbose)
            time.sleep(args.loop)
    else:
        run_alert_db_size(db_engine, args.max_db_size_in_gb,
                          args.log_level_percentage,
                          args.warning_level_percentage,
                          args.critical_level_percentage,
                          args.log_tag,
                          print_status=args.hide_status,
                          verbose=args.verbose)


if __name__ == "__main__":
    main()
