--
-- PostgreSQL database dump
--

SET statement_timeout = 0;
SET lock_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

--
-- Name: plpgsql; Type: EXTENSION; Schema: -; Owner: -
--

-- CREATE EXTENSION IF NOT EXISTS plpgsql WITH SCHEMA pg_catalog;


--
-- Name: EXTENSION plpgsql; Type: COMMENT; Schema: -; Owner: -
--

-- COMMENT ON EXTENSION plpgsql IS 'PL/pgSQL procedural language';


SET search_path = public, pg_catalog;

--
-- Name: add_data_item(character varying, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_data_item(p_data_item character varying, p_data_type integer) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_data_item_id integer := 0;

BEGIN
    insert into data_item(
        name,
        data_type)
    values(
        p_data_item,
        p_data_type);

    select data_item_id into v_data_item_id
    from data_item
    where name = p_data_item;

    return v_data_item_id;
END

$$;


--
-- Name: add_data_sample(integer, integer, timestamp without time zone, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_data_sample(p_node_id integer, p_data_item_id integer, p_time_stamp timestamp without time zone, p_value_num double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    insert into data_sample(
        node_id,
        data_item_id,
        time_stamp,
        value_num,
        value_str,
        units)
    values(
        p_node_id,
        p_data_item_id,
        p_time_stamp,
        p_value_num,
        p_value_str,
        p_units);

    return;
END
$$;


--
-- Name: add_feature(character varying, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_feature(p_feature character varying, p_data_type integer) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_feature_id integer := 0;

BEGIN
    insert into feature(
        name,
        data_type)
    values(
        p_feature,
        p_data_type);

    select feature_id into v_feature_id
    from feature
    where name = p_feature;

    return v_feature_id;
END

$$;


--
-- Name: add_node(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_node(p_hostname character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_node_id integer := 0;

BEGIN
    insert into node(hostname)
    values(p_hostname);

    select node_id into v_node_id
    from node
    where hostname = p_hostname;

    return v_node_id;
END

$$;


--
-- Name: add_node_feature(integer, integer, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION add_node_feature(p_node_id integer, p_feature_id integer, p_value_num double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    insert into node_feature(
        node_id,
        feature_id,
        value_num,
        value_str,
        units)
    values(
        p_node_id,
        p_feature_id,
        p_value_num,
        p_value_str,
        p_units);

    return;
END
$$;


--
-- Name: get_data_item_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_data_item_id(p_data_item character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_data_item_id integer := 0;

BEGIN
    -- Get the data item ID
    select data_item_id into v_data_item_id
    from data_item
    where name = p_data_item;

    -- Does it exist?
    if (v_data_item_id is null) then
        v_data_item_id := 0;
    end if;

    return v_data_item_id;
END
$$;


--
-- Name: get_feature_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_feature_id(p_feature character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_feature_id integer := 0;

BEGIN
    -- Get the feature ID
    select feature_id into v_feature_id
    from feature
    where name = p_feature;

    -- Does it exist?
    if (v_feature_id is null) then
        v_feature_id := 0;
    end if;

    return v_feature_id;
END
$$;


--
-- Name: get_node_id(character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION get_node_id(p_hostname character varying) RETURNS integer
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_node_id integer := 0;

BEGIN
    -- Get the node ID
    select node_id into v_node_id
    from node
    where hostname = p_hostname;

    -- Does it exist?
    if (v_node_id is null) then
        v_node_id := 0;
    end if;

    return v_node_id;
END

$$;


--
-- Name: node_feature_exists(integer, integer); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION node_feature_exists(p_node_id integer, p_feature_id integer) RETURNS boolean
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_exists boolean := false;

BEGIN

    v_exists := exists(
        select 1
        from node_feature
        where node_id = p_node_id and feature_id = p_feature_id);

    return v_exists;
END
$$;


--
-- Name: record_data_sample(character varying, character varying, character varying, timestamp without time zone, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION record_data_sample(p_hostname character varying, p_data_group character varying, p_data_item character varying, p_time_stamp timestamp without time zone, p_value_num double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_node_id integer := 0;
    v_data_item_id integer := 0;
    v_data_item_name varchar(250);
    v_data_type integer := 0;

BEGIN
    -- Compose the data item name from the data group and item itself
    v_data_item_name := p_data_group || '_';
    v_data_item_name := v_data_item_name || p_data_item;

    -- Determine the value type
    if (p_value_num is not null and p_value_str is null) then
        v_data_type = 1; -- Numeric data type
    elseif (p_value_str is not null and p_value_num is null) then
        v_data_type = 2;
    else
        raise exception 'No value passed or ambiguous value defined';
        raise SQLSTATE '45000';
    end if;

    -- Get the node ID
    v_node_id := get_node_id(p_hostname);
    if (v_node_id = 0) then
        v_node_id := add_node(p_hostname);
    end if;

    -- Get the data item ID
    v_data_item_id := get_data_item_id(v_data_item_name);
    if (v_data_item_id = 0) then
        v_data_item_id := add_data_item(v_data_item_name, v_data_type);
    end if;

    -- Add the data sample
    perform add_data_sample(
        v_node_id,
        v_data_item_id,
        p_time_stamp,
        p_value_num,
        p_value_str, p_units);

    return;
END;
$$;


--
-- Name: set_node_feature(character varying, character varying, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION set_node_feature(p_hostname character varying, p_feature character varying, p_value_num double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$

DECLARE
    v_node_id integer := 0;
    v_feature_id integer := 0;
    v_data_type integer := 0;
    v_node_feature_exists boolean := true;

BEGIN
    -- Determine the value type
    if (p_value_num is not null and p_value_str is null) then
        v_data_type := 1; -- Numeric data type
    elseif (p_value_str is not null and p_value_num is null) then
        v_data_type := 2; -- String data type
    else
        raise exception 'No value passed or ambiguous value defined';
        raise SQLSTATE '45000';
    end if;

    -- Get the node ID
    v_node_id := get_node_id(p_hostname);
    if (v_node_id = 0) then
        v_node_id := add_node(p_hostname);
        v_node_feature_exists := false;
    end if;

    -- Get the feature ID
    v_feature_id := get_feature_id(p_feature);
    if (v_feature_id = 0) then
        v_feature_id := add_feature(p_feature, v_data_type);
        v_node_feature_exists := false;
    end if;

    -- Has this feature already been added for this node?
    if (v_node_feature_exists) then
        v_node_feature_exists := node_feature_exists(v_node_id, v_feature_id);
    end if;

    if (v_node_feature_exists) then
        -- If so, just update it
        perform update_node_feature(
            v_node_id,
            v_feature_id,
            p_value_num,
            p_value_str,
            p_units);
    else        
        -- If not, add it
        perform add_node_feature(
            v_node_id,
            v_feature_id,
            p_value_num,
            p_value_str,
            p_units);
    end if;

    return;
END

$$;


--
-- Name: update_node_feature(integer, integer, double precision, character varying, character varying); Type: FUNCTION; Schema: public; Owner: -
--

CREATE FUNCTION update_node_feature(p_node_id integer, p_feature_id integer, p_value_num double precision, p_value_str character varying, p_units character varying) RETURNS void
    LANGUAGE plpgsql
    AS $$
BEGIN
    update node_feature
    set value_num = p_value_num,
        value_str = p_value_str,
        units = p_units
    where node_id = p_node_id and feature_id = p_feature_id;
END
$$;


SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: data_item; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE data_item (
    data_item_id integer NOT NULL,
    name character varying(250) NOT NULL,
    data_type integer DEFAULT 1 NOT NULL,
    CONSTRAINT data_type CHECK (((data_type = 1) OR (data_type = 2)))
);


--
-- Name: data_item_data_item_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE data_item_data_item_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: data_item_data_item_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE data_item_data_item_id_seq OWNED BY data_item.data_item_id;


--
-- Name: data_sample; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE data_sample (
    node_id integer NOT NULL,
    data_item_id integer NOT NULL,
    time_stamp timestamp without time zone NOT NULL,
    value_num double precision,
    value_str character varying(50),
    units character varying(50),
    CONSTRAINT value CHECK (((value_num IS NOT NULL) OR (value_str IS NOT NULL)))
);


--
-- Name: node; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE node (
    node_id integer NOT NULL,
    hostname character varying NOT NULL,
    node_type smallint,
    status smallint,
    num_cpus smallint,
    num_sockets smallint,
    cores_per_socket smallint,
    threads_per_core smallint,
    memory smallint
);


--
-- Name: data_samples_view; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW data_samples_view AS
 SELECT data_sample.node_id,
    node.hostname,
    data_sample.data_item_id,
    data_item.name AS data_item,
    data_item.data_type,
    data_sample.time_stamp,
    data_sample.value_num,
    data_sample.value_str,
    data_sample.units
   FROM ((data_sample
     JOIN node ON ((node.node_id = data_sample.node_id)))
     JOIN data_item ON ((data_item.data_item_id = data_sample.data_item_id)));


--
-- Name: event; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE event (
    node_id integer NOT NULL,
    event_type_id integer NOT NULL,
    time_stamp timestamp without time zone NOT NULL,
    description text NOT NULL
);


--
-- Name: event_type; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE event_type (
    event_type_id integer NOT NULL,
    name character varying(250)
);


--
-- Name: event_type_event_type_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE event_type_event_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: event_type_event_type_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE event_type_event_type_id_seq OWNED BY event_type.event_type_id;


--
-- Name: feature; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE feature (
    feature_id integer NOT NULL,
    name character varying(250) NOT NULL,
    data_type integer DEFAULT 1 NOT NULL,
    CONSTRAINT data_type CHECK (((data_type = 1) OR (data_type = 2)))
);


--
-- Name: feature_feature_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE feature_feature_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: feature_feature_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE feature_feature_id_seq OWNED BY feature.feature_id;


--
-- Name: fru; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE fru (
    node_id integer NOT NULL,
    fru_type_id integer NOT NULL,
    fru_id integer NOT NULL,
    serial_number character varying(50) NOT NULL
);


--
-- Name: fru_fru_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE fru_fru_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: fru_fru_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE fru_fru_id_seq OWNED BY fru.fru_id;


--
-- Name: fru_type; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE fru_type (
    fru_type_id integer NOT NULL,
    name character varying(250) NOT NULL
);


--
-- Name: fru_type_fru_type_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE fru_type_fru_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: fru_type_fru_type_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE fru_type_fru_type_id_seq OWNED BY fru_type.fru_type_id;


--
-- Name: job; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE job (
    job_id integer NOT NULL,
    name character varying(100),
    num_nodes integer NOT NULL,
    username character varying(75) NOT NULL,
    time_submitted timestamp without time zone NOT NULL,
    start_time timestamp without time zone,
    end_time timestamp without time zone,
    energy_usage real,
    exit_status integer
);


--
-- Name: job_job_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE job_job_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: job_job_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE job_job_id_seq OWNED BY job.job_id;


--
-- Name: job_node; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE job_node (
    job_id integer NOT NULL,
    node_id integer NOT NULL
);


--
-- Name: maintenance_record; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE maintenance_record (
    node_id integer NOT NULL,
    fru_type_id integer NOT NULL,
    fru_id integer NOT NULL,
    replacement_date date NOT NULL,
    old_serial_number character varying(50) NOT NULL,
    new_serial_number character varying(50) NOT NULL
);


--
-- Name: node_feature; Type: TABLE; Schema: public; Owner: -; Tablespace: 
--

CREATE TABLE node_feature (
    node_id integer NOT NULL,
    feature_id integer NOT NULL,
    value_num double precision,
    value_str character varying(100),
    units character varying(50),
    CONSTRAINT value CHECK (((value_num IS NOT NULL) OR (value_str IS NOT NULL)))
);


--
-- Name: node_node_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE node_node_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: node_node_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE node_node_id_seq OWNED BY node.node_id;


--
-- Name: data_item_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_item ALTER COLUMN data_item_id SET DEFAULT nextval('data_item_data_item_id_seq'::regclass);


--
-- Name: event_type_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY event_type ALTER COLUMN event_type_id SET DEFAULT nextval('event_type_event_type_id_seq'::regclass);


--
-- Name: feature_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY feature ALTER COLUMN feature_id SET DEFAULT nextval('feature_feature_id_seq'::regclass);


--
-- Name: fru_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY fru ALTER COLUMN fru_id SET DEFAULT nextval('fru_fru_id_seq'::regclass);


--
-- Name: fru_type_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY fru_type ALTER COLUMN fru_type_id SET DEFAULT nextval('fru_type_fru_type_id_seq'::regclass);


--
-- Name: job_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY job ALTER COLUMN job_id SET DEFAULT nextval('job_job_id_seq'::regclass);


--
-- Name: node_id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY node ALTER COLUMN node_id SET DEFAULT nextval('node_node_id_seq'::regclass);


--
-- Name: data_item_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_item
    ADD CONSTRAINT data_item_pkey PRIMARY KEY (data_item_id);


--
-- Name: data_sample_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_sample
    ADD CONSTRAINT data_sample_pkey PRIMARY KEY (node_id, data_item_id, time_stamp);


--
-- Name: event_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY event
    ADD CONSTRAINT event_pkey PRIMARY KEY (node_id, event_type_id, time_stamp);


--
-- Name: event_type_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY event_type
    ADD CONSTRAINT event_type_pkey PRIMARY KEY (event_type_id);


--
-- Name: feature_name_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY feature
    ADD CONSTRAINT feature_name_key UNIQUE (name);


--
-- Name: feature_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY feature
    ADD CONSTRAINT feature_pkey PRIMARY KEY (feature_id);


--
-- Name: fru_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY fru
    ADD CONSTRAINT fru_pkey PRIMARY KEY (node_id, fru_type_id, fru_id);


--
-- Name: fru_type_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY fru_type
    ADD CONSTRAINT fru_type_pkey PRIMARY KEY (fru_type_id);


--
-- Name: job_node_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY job_node
    ADD CONSTRAINT job_node_pkey PRIMARY KEY (job_id, node_id);


--
-- Name: job_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY job
    ADD CONSTRAINT job_pkey PRIMARY KEY (job_id);


--
-- Name: maintenance_record_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY maintenance_record
    ADD CONSTRAINT maintenance_record_pkey PRIMARY KEY (node_id, fru_type_id, fru_id, replacement_date);


--
-- Name: node_feature_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT node_feature_pkey PRIMARY KEY (node_id, feature_id);


--
-- Name: node_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node
    ADD CONSTRAINT node_pkey PRIMARY KEY (node_id);


--
-- Name: unique_host; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY node
    ADD CONSTRAINT unique_host UNIQUE (hostname);


--
-- Name: unique_name; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace: 
--

ALTER TABLE ONLY data_item
    ADD CONSTRAINT unique_name UNIQUE (name);


--
-- Name: data_sample_data_item_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_sample
    ADD CONSTRAINT data_sample_data_item_id_fkey FOREIGN KEY (data_item_id) REFERENCES data_item(data_item_id);


--
-- Name: data_sample_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY data_sample
    ADD CONSTRAINT data_sample_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: event_event_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY event
    ADD CONSTRAINT event_event_type_id_fkey FOREIGN KEY (event_type_id) REFERENCES event_type(event_type_id);


--
-- Name: event_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY event
    ADD CONSTRAINT event_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: fru_fru_type_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY fru
    ADD CONSTRAINT fru_fru_type_id_fkey FOREIGN KEY (fru_type_id) REFERENCES fru_type(fru_type_id);


--
-- Name: fru_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY fru
    ADD CONSTRAINT fru_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: job_node_job_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY job_node
    ADD CONSTRAINT job_node_job_id_fkey FOREIGN KEY (job_id) REFERENCES job(job_id);


--
-- Name: job_node_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY job_node
    ADD CONSTRAINT job_node_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: maintenance_record_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY maintenance_record
    ADD CONSTRAINT maintenance_record_node_id_fkey FOREIGN KEY (node_id, fru_type_id, fru_id) REFERENCES fru(node_id, fru_type_id, fru_id);


--
-- Name: node_feature_feature_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT node_feature_feature_id_fkey FOREIGN KEY (feature_id) REFERENCES feature(feature_id);


--
-- Name: node_feature_node_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY node_feature
    ADD CONSTRAINT node_feature_node_id_fkey FOREIGN KEY (node_id) REFERENCES node(node_id);


--
-- Name: public; Type: ACL; Schema: -; Owner: -
--

-- REVOKE ALL ON SCHEMA public FROM PUBLIC;
-- REVOKE ALL ON SCHEMA public FROM postgres;
-- GRANT ALL ON SCHEMA public TO postgres;
-- GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- PostgreSQL database dump complete
--

