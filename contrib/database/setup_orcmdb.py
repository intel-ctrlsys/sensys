#
# Copyright (c) 2015 Intel Corporation. All rights reserved
#
from argparse import ArgumentParser
import ConfigParser
from sqlalchemy.exc import OperationalError
import os

import models


def main():
    """Command line interface for setup database"""
    parser = ArgumentParser(description="Setup ORCM database")
    parser.add_argument("--alembic-ini", dest='alembic_ini',
                        metavar='/path/to/alembic/config/file',
                        type=str, default="schema_migration.ini",
                        help="A path to the Alembic config file")
    args = parser.parse_args()

    save_cwd = os.getcwd()
    ini_dir, ini_file = os.path.split(args.alembic_ini)
    try:
        os.chdir(os.path.join(save_cwd, ini_dir))
        models.setup(alembic_ini=ini_file)
    except OperationalError as e:
        print("\nException: %s\n" % e)
        print("Make sure the database exists and the current user has "
              "proper permissions to create tables and issue DDL & DML "
              "statements.")
    finally:
        os.chdir(save_cwd)


if __name__ == "__main__":
    main()
