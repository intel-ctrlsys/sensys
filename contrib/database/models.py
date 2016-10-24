#
# Copyright (c) 2015-2016 Intel Corporation. All rights reserved
#
"""This module contains a set of classes that used by SQLAlchemy for
Object Relational Mapper (ORM)

"""
from __future__ import print_function
from distutils.version import StrictVersion
import os
import alembic
from alembic.config import Config
from alembic import command
import logging
import sqlalchemy as sa
from sqlalchemy.orm import relationship, sessionmaker, backref
from sqlalchemy import BigInteger, Column, create_engine, CheckConstraint
from sqlalchemy import Date, DateTime
from sqlalchemy import Float, ForeignKey, ForeignKeyConstraint, Integer, String
from sqlalchemy import SmallInteger, Table, Text
from sqlalchemy.ext import declarative, compiler

# disabling Pylint error messages on SQLAlchemy classes and convention
# Invalid constant name.
# pylint: disable=C0103
# Too few public methods.
# pylint: disable=R0903
# Too many public methods.
# pylint: disable=R0904
# Class has no __init__ method.
# pylint: disable=W0232
# Redefining name '<name>' from outer scope.
# pylint: disable=W0621

# Minimum required version of SQLAlchemy and Alembic for the Database
# migration code to work correctly.  Specifically the naming convention feature
# that are necessary to provide predictable database constraints' names.
if StrictVersion(alembic.__version__) < StrictVersion("0.8.2"):
    raise RuntimeError("The Alembic version need to be at least 0.8.2.  "
                       "The installed version is %s" % alembic.__version__)
if StrictVersion(sa.__version__) < StrictVersion("0.9.2"):
    raise RuntimeError("The SQLAlchemy version need to be at least 0.9.2.  "
                       "The installed version is %s" % sa.__version__)


log = logging.getLogger(__name__)
log.setLevel(logging.DEBUG)


# Define a generic current timestamp function to be execute on the database
# server side
class CurrentTimeStamp(sa.sql.expression.FunctionElement):
    """Define a current timestamp function name that can be use to map to
    different database dialect implementations.
    """
    # Too many ancestors (12/7)
    # pylint: disable=R0901
    type = DateTime()


@compiler.compiles(CurrentTimeStamp, 'postgresql')
def postgres_current_timestamp(element, compiler, **kw):
    """Return the Postgres or MySQL function call to get the current DB
    server time

    :param element:  The element being construct
    :param compiler:  The compiler for postgresql
    :param kw:  Any additional argument
    :return: Postgres or MySQL function name for generating current timestamp
    """
    # Unused variable
    # pylint: disable=W0613
    return "CURRENT_TIMESTAMP"


Base = declarative.declarative_base()

# Naming convention for SQLAlchemy to auto generate the constrain name to
# have a predictable name.  Important for referencing constrains by name
# (e.g. alembic downgrade script).
Base.metadata.naming_convention = {
    "ix": 'ix_%(column_0_label)s',
    "uq": "uq_%(table_name)s_%(column_0_name)s",
    "ck": "ck_%(table_name)s_%(constraint_name)s",
    "fk": "fk_%(table_name)s_%(column_0_name)s_%(referred_table_name)s",
    "pk": "pk_%(table_name)s"}

metadata = Base.metadata

# Default session
session = sessionmaker()()


class DataItem(Base):
    """data_item table"""
    __tablename__ = 'data_item'

    data_item_id = Column(Integer, primary_key=True, autoincrement=True,
                          nullable=False)
    name = Column(String(250), nullable=False, unique=True)
    data_type_id = Column(Integer, ForeignKey('data_type.data_type_id'),
                          nullable=False)
    data_type = relationship('DataType')


class DataSample(Base):
    """data_sample table"""
    __tablename__ = 'data_sample'
    __table_args__ = (tuple(CheckConstraint('(value_int  IS NOT NULL OR '
                                            ' value_real IS NOT NULL OR '
                                            ' value_str  IS NOT NULL)',
                                            name='at_least_one_value')))

    node_id = Column(Integer, ForeignKey('node.node_id'), primary_key=True,
                     nullable=False)
    node = relationship('Node')
    data_item_id = Column(Integer, ForeignKey('data_item.data_item_id'),
                          primary_key=True, nullable=False)
    data_item = relationship('DataItem')
    time_stamp = Column(DateTime, primary_key=True, nullable=False)
    value_int = Column(BigInteger)
    value_real = Column(Float(53))
    value_str = Column(String(500))
    units = Column(String(50))


class DataSampleRaw(Base):
    """data_sample_raw table"""
    __tablename__ = 'data_sample_raw'
    __table_args__ = (tuple(CheckConstraint('(value_int  IS NOT NULL OR '
                                            ' value_real IS NOT NULL OR '
                                            ' value_str  IS NOT NULL)',
                                            name='at_least_one_value')))
    data_sample_id = Column(Integer, primary_key=True, autoincrement=True)
    hostname = Column(String(256), nullable=False)
    data_item = Column(String(250), nullable=False)
    time_stamp = Column(DateTime, nullable=False)
    value_int = Column(BigInteger)
    value_real = Column(Float(53))
    value_str = Column(String(500))
    units = Column(String(50))
    data_type_id = Column(Integer, nullable=False)
    # The 'app_value_type_id' To be replacing the 'data_type_id' once the rest
    # of the code has been refactored use this new column name
    app_value_type_id = Column(Integer, nullable=False)
    # Relationship: Many-To-One; parent: Event; child: EventData
    # Many EventData associated to (One) Event
    #   One Event has Many EventData
    event_id = Column(Integer, ForeignKey("event.event_id"))
    event = sa.orm.relationship("Event", lazy="joined",
                                backref=backref("event_data",
                                                order_by=data_sample_id))


class DataType(Base):
    """data_type table"""
    __tablename__ = 'data_type'

    data_type_id = Column(Integer, primary_key=True)
    name = Column(String(150), unique=True)


class DiagSubtype(Base):
    """diag_subtype table"""
    __tablename__ = 'diag_subtype'

    diag_subtype_id = Column(Integer, primary_key=True, autoincrement=True,
                             nullable=False)
    name = Column(String(250), nullable=False, unique=True)
    diag_types = relationship(u'DiagType', secondary='diag')


class Diag(Base):
    """diag table"""
    __tablename__ = 'diag'

    diag_type_id = Column(Integer, ForeignKey('diag_type.diag_type_id'),
                          primary_key=True, nullable=False)
    diag_type = relationship('DiagType')
    diag_subtype_id = Column(Integer,
                             ForeignKey('diag_subtype.diag_subtype_id'),
                             primary_key=True, nullable=False)
    diag_subtype = relationship('DiagSubtype')


class DiagTest(Base):
    """diag_test table"""
    __tablename__ = 'diag_test'

    node_id = Column(Integer, ForeignKey('node.node_id'),
                     primary_key=True, nullable=False)
    node = relationship('Node')
    diag_type_id = Column(Integer, ForeignKey('diag_type.diag_type_id'),
                          primary_key=True, nullable=False)
    diag_type = relationship('DiagType')
    diag_subtype_id = Column(Integer,
                             ForeignKey('diag_subtype.diag_subtype_id'),
                             primary_key=True, nullable=False)
    diag_subtype = relationship('DiagSubtype')

    start_time = Column(DateTime, primary_key=True, nullable=False)
    end_time = Column(DateTime)
    component_index = Column(Integer)
    test_result_id = Column(Integer, nullable=False)


class DiagTestConfig(Base):
    """diag_test_config table"""
    __tablename__ = 'diag_test_config'
    __table_args__ = ((ForeignKeyConstraint(
                           ['node_id', 'diag_type_id', 'diag_subtype_id',
                            'start_time'],
                           ['diag_test.node_id', 'diag_test.diag_type_id',
                            'diag_test.diag_subtype_id',
                            'diag_test.start_time']),
                       CheckConstraint(
                           '(value_int  IS NOT NULL OR '
                           ' value_real IS NOT NULL OR '
                           ' value_str  IS NOT NULL)',
                           name='at_least_one_value')))

    node_id = Column(Integer, primary_key=True, nullable=False)
    diag_type_id = Column(Integer, primary_key=True, nullable=False)
    diag_subtype_id = Column(Integer, primary_key=True, nullable=False)
    start_time = Column(DateTime, primary_key=True, nullable=False)
    test_param_id = Column(Integer, ForeignKey('test_param.test_param_id'),
                           primary_key=True, nullable=False)
    value_real = Column(Float(53))
    value_str = Column(String(100))
    units = Column(String(50))
    value_int = Column(BigInteger)

    node = relationship('DiagTest')
    test_param = relationship('TestParam')


class DiagType(Base):
    """diag_type table"""
    __tablename__ = 'diag_type'

    diag_type_id = Column(Integer, primary_key=True, autoincrement=True,
                          nullable=False)
    name = Column(String(250), nullable=False, unique=True)


class Event(Base):
    """The event table is to store required attributes of an event"""
    __tablename__ = 'event'

    event_id = Column(Integer, nullable=False, primary_key=True,
                      autoincrement=True)
    time_stamp = Column(DateTime, nullable=False, server_default=sa.func.now())
    severity = Column(String(50), nullable=False, server_default='')
    type = Column(String(50), nullable=False, server_default='')
    version = Column(String(250), nullable=False, server_default='')
    vendor = Column(String(250), nullable=False, server_default='')
    description = Column(Text, nullable=False, server_default='')


class EventData(Base):
    """The event_data table is to store all the key value pairs of a given
    event.
    """

    __tablename__ = 'event_data'

    event_data_id = Column(Integer, primary_key=True, autoincrement=True,
                           nullable=False)
    # Relationship: Many-To-One; parent: Event; child: EventData
    # Many EventData associated to (One) Event
    #   One Event has Many EventData
    event_id = Column(Integer,
                      ForeignKey('event.event_id'), nullable=False)
    event = sa.orm.relationship('Event', lazy='joined',
                                backref=backref('event_data',
                                                order_by=event_data_id))
    # Relationship: Many-To-One; parent: EventDataKey; child: EventData
    #   Many EventData associated to (One) EventDataKey
    #   One EventDataKey has Many EventData
    event_data_key_id = Column(Integer,
                               ForeignKey('event_data_key.event_data_key_id'),
                               nullable=False)
    event_data_key = sa.orm.relationship('EventDataKey', lazy='joined',
                                         backref=backref('event_data',
                                                         order_by=event_data_id)
                                        )
    value_int = Column(BigInteger)
    value_real = Column(Float(53))
    value_str = Column(String(500))
    units = Column(String(50), nullable=False, server_default='')
    CheckConstraint('(value_int  IS NOT NULL OR '
                    ' value_real IS NOT NULL OR '
                    ' value_str  IS NOT NULL)',
                    name='at_least_one_value')


class EventDataKey(Base):
    """The event_data_key table is to store all the key names and the
    application data type of that key
    """
    __tablename__ = 'event_data_key'

    event_data_key_id = Column(Integer, primary_key=True, autoincrement=True,
                               nullable=False)
    name = Column(String(250), unique=True)
    # The app_value_type_id is to store the ID of the value type provided by the
    # application.
    app_value_type_id = Column(Integer, nullable=False)


class Feature(Base):
    """feature table"""
    __tablename__ = 'feature'

    feature_id = Column(Integer, primary_key=True, autoincrement=True,
                        nullable=False)
    name = Column(String(250), nullable=False, unique=True)
    data_type_id = Column(Integer, ForeignKey('data_type.data_type_id'),
                          nullable=False)

    data_type = relationship('DataType')


class Fru(Base):
    """fru table"""
    __tablename__ = 'fru'

    node_id = Column(Integer, ForeignKey('node.node_id'), primary_key=True,
                     nullable=False)
    fru_type_id = Column(Integer, ForeignKey('fru_type.fru_type_id'),
                         primary_key=True,
                         nullable=False)
    fru_id = Column(Integer, primary_key=True, autoincrement=True,
                    nullable=False)
    serial_number = Column(String(50), nullable=False)

    fru_type = relationship('FruType')
    node = relationship('Node')


class FruType(Base):
    """fru_type table"""
    __tablename__ = 'fru_type'

    fru_type_id = Column(Integer, primary_key=True, autoincrement=True,
                         nullable=False)
    name = Column(String(250), nullable=False)


class Job(Base):
    """job table"""
    __tablename__ = 'job'

    job_id = Column(Integer, primary_key=True, autoincrement=True,
                    nullable=False)
    name = Column(String(100))
    num_nodes = Column(Integer, nullable=False)
    username = Column(String(75), nullable=False)
    time_submitted = Column(DateTime, nullable=False)
    start_time = Column(DateTime)
    end_time = Column(DateTime)
    energy_usage = Column(Float)
    exit_status = Column(Integer)
    nodes = relationship('Node', secondary='job_node')


Table('job_node',
      Base.metadata,
      Column('job_id', Integer, ForeignKey('job.job_id')),
      Column('node_id', Integer, ForeignKey('node.node_id')))


class MaintenanceRecord(Base):
    """maintenance_record table"""
    __tablename__ = 'maintenance_record'
    __table_args__ = (tuple(ForeignKeyConstraint(
        ['node_id', 'fru_type_id', 'fru_id'],
        ['fru.node_id', 'fru.fru_type_id', 'fru.fru_id'])))

    node_id = Column(Integer, primary_key=True, nullable=False)
    fru_type_id = Column(Integer, primary_key=True, nullable=False)
    fru_id = Column(Integer, primary_key=True, nullable=False)
    replacement_date = Column(Date, primary_key=True, nullable=False)
    old_serial_number = Column(String(50), nullable=False)
    new_serial_number = Column(String(50), nullable=False)


class Node(Base):
    """node table"""
    __tablename__ = 'node'

    node_id = Column(Integer, primary_key=True, autoincrement=True,
                     nullable=False)
    hostname = Column(String(256), nullable=False, unique=True)
    node_type = Column(SmallInteger)
    status = Column(SmallInteger)
    num_cpus = Column(SmallInteger)
    num_sockets = Column(SmallInteger)
    cores_per_socket = Column(SmallInteger)
    threads_per_core = Column(SmallInteger)
    memory = Column(SmallInteger)


class NodeFeature(Base):
    """node_feature table"""
    __tablename__ = 'node_feature'

    node_id = Column(Integer, ForeignKey('node.node_id'), primary_key=True,
                     nullable=False)
    feature_id = Column(Integer, ForeignKey('feature.feature_id'),
                        primary_key=True, nullable=False)
    value_real = Column(Float(53))
    value_str = Column(String(100))
    units = Column(String(50))
    value_int = Column(BigInteger)
    time_stamp = Column(DateTime, nullable=False)

    feature = relationship('Feature')
    node = relationship('Node')


class TestParam(Base):
    """test_param table"""
    __tablename__ = 'test_param'

    test_param_id = Column(Integer, primary_key=True, autoincrement=True,
                           nullable=False)
    name = Column(String(250), nullable=False, unique=True)
    data_type_id = Column(Integer, ForeignKey('data_type.data_type_id'),
                          nullable=False)

    data_type = relationship('DataType')


class TestResult(Base):
    """test result table"""
    __tablename__ = 'test_result'

    test_result_id = Column(Integer, primary_key=True, autoincrement=True,
                            nullable=False)
    name = Column(String(250), nullable=False, unique=True)


def setup(alembic_ini="schema_migration.ini"):
    """Setup all SQLAlchemy sessions and connections

    :param db_url: The database URL as required by SQLAlchemy
    :param alembic_ini: Alembic config file
    :return: None
    """
    alembic_conf = Config(alembic_ini)
    db_url = alembic_conf.get_section_option("alembic", "sqlalchemy.url")
    if not db_url:
        db_url = os.getenv("PG_DB_URL")
    if not db_url:
        raise RuntimeError("The db_url is not in the kwarg "
                           "nor in the alembic_ini file.")
    sqlalchemy_db_url = sa.engine.url.make_url(db_url)
    print("-" * 50)
    msg = ("Connecting to '%s' database on %s server at '%s'" % (
        sqlalchemy_db_url.database,
        sqlalchemy_db_url.get_dialect().name.upper(),
        sqlalchemy_db_url.host))
    print(msg)
    engine = create_engine(db_url)
    Base.metadata.bind = engine
    # Base.metadata.bind.echo = True   # Log the SQL interaction.
    msg = (engine.execute("select 'Connected to %s database'" %
                          sqlalchemy_db_url.database).scalar())
    print(msg)
    msg = ("Established connection to '%s' database" %
           sqlalchemy_db_url.database)
    print(msg)

    print("-" * 50)
    print("Starting Alembic upgrade database to latest version")
    command.upgrade(alembic_conf, "head", tag=db_url)
    print("Completed running Alembic command")
    print("-" * 50)


if __name__ == "__main__":
    import pprint
    setup()
    dummy_query = session.query(DataSampleRaw).limit(10)
    print("Sample list of  DataSampleRaw")
    print(pprint.pformat(dummy_query.all()))
