#
# Copyright (c) 2015-2016 Intel Corporation. All rights reserved
#
from __future__ import with_statement
import logging
from alembic import context
from sqlalchemy import create_engine, pool
import sys
import os

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import models


logging.basicConfig(level=logging.INFO)

# this is the Alembic Config object, which provides
# access to the values within the .ini file in use.
config = context.config
config_url = config.get_main_option('sqlalchemy.url')

# add your model's MetaData object here
# for 'autogenerate' support
target_metadata = models.Base.metadata

# other values from the config, defined by the needs of env.py,
# can be acquired:
# my_important_option = config.get_main_option("my_important_option")
# ... etc.

# support for passing database URL on the command line
# usage:  alembic -x db_url=postgresql://localhost/orcmdb upgrade head
cmd_line_url = context.get_x_argument(as_dictionary=True).get('db_url')
# support passing database URL as tag in upgrade() function call.
# usage:  command.upgrade(Config(alembic_ini), "head", tag=db_url)
tag_url = context.get_tag_argument()

missing_db_url_msg = ("Please set the database connection string in "
                      "either 'PG_DB_URL' environment variable or specify "
                      "it in the schema_migration config file under "
                      "'sqlalchemy.url'.\nConnection string pattern:\n"
                      "postgresql[+<driver>://[<username>[:<password>]]"
                      "@<server>[:<port>]/<database>\n\n"
                      "http://docs.sqlalchemy.org/en/latest/core/"
                      "engines.html#database-urls")

DB_URL_ENVIRONMENT_VAR = 'PG_DB_URL'

def run_migrations_offline():
    """Run migrations in 'offline' mode.

    This configures the context with just a URL
    and not an Engine, though an Engine is acceptable
    here as well.  By skipping the Engine creation
    we don't even need a DBAPI to be available.

    Calls to context.execute() here emit the given string to the
    script output.
    """
    if cmd_line_url:
        url = cmd_line_url
    elif tag_url:
        url = tag_url
    elif config_url:
        url = config_url
    else:
        url = os.getenv(DB_URL_ENVIRONMENT_VAR)

    if url:
        context.configure(url=url)
    else:
        raise RuntimeError(missing_db_url_msg)

    with context.begin_transaction():
        context.run_migrations()

def run_migrations_online():
    """Run migrations in 'online' mode.

    In this scenario we need to create an Engine
    and associate a connection with the context.
    """
    if cmd_line_url:
        url = cmd_line_url
    elif tag_url:
        url = tag_url
    elif config_url:
        url = config_url
    else:
        url = os.getenv(DB_URL_ENVIRONMENT_VAR)

    if url:
        engine = create_engine(url, poolclass=pool.NullPool)
    else:
        raise RuntimeError(missing_db_url_msg)

    connection = engine.connect()
    context.configure(connection=connection, target_metadata=target_metadata,
                      compare_type=True)

    try:
        with context.begin_transaction():
            context.run_migrations()
    finally:
        connection.close()


if context.is_offline_mode():
    run_migrations_offline()
else:
    run_migrations_online()

