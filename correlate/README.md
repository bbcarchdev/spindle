# spindle-correlate

This module processes co-references and stores information about them, either
in an RDF store via SPARQL Update, or into a relational database, depending
upon configuration.

When a graph is passed to this processor by Twine, it examines the contents
of the graph and looks for co-reference assertions (typically `owl:sameAs`
statements, but see the "Rule-base" section below). Where some are
found, `spindle-correlate` generates "proxy" entities within a named graph that
connect together all of the URIs for a given entity.

For example, if graphs are processed which assert that `<A> owl:sameAs <B>`,
`<B> owl:sameAs <C>` and `<C> owl:sameAs <D>`, the end result is the following
set of statements:

	<http://spindle.example.com/> {
		
		<A> owl:sameAs <http://spindle.example.com/57345004aa474a8d85df61a8bbc6536b#id> .
		<B> owl:sameAs <http://spindle.example.com/57345004aa474a8d85df61a8bbc6536b#id> .
		<C> owl:sameAs <http://spindle.example.com/57345004aa474a8d85df61a8bbc6536b#id> .
		<D> owl:sameAs <http://spindle.example.com/57345004aa474a8d85df61a8bbc6536b#id> .
		
	}

Note that the base URI, `http://spindle.example.com/` in the above example,
is configurable (see the "Configuration" section below). The hexadecimal string
following the base URI (`57345004aa474a8d85df61a8bbc6536b`) is a UUID that is
generated automatically by `spindle-correlate` for each distinct entity whose
data is processed.

## Use with a relational database

By default, `spindle-correlate` writes triples (as described above) into the
configured named graph using SPARQL Update, via the SPARQL connection details
that Twine is configured to use. As an alternative, `spindle-correlate` can
write co-reference information into a PostgreSQL database (see the
"Configuration" section below for details).

When configured this way, the database schema will be updated automatically
(or manually via `twine -S`) to create the relations required for both
`spindle-correlate` and `spindle-generate`.

Co-references are written to the `proxy` database table, which maps an
`id` (a UUID) to an array of URIs. A single row might look like:

	*************************** 1. row ***************************
		id: 97864996-2c6f-4a06-a9b7-a8c6b15548af
		sameas: {http://sws.geonames.org/2643743/,http://dbpedia.org/resource/London}

In addition:

* The `moved` table will be populated when two entities are merged: whichever
of the two identifiers ceases to exist after the merge will be added to the
`from` column, while the newly-merged identifier will be placed into the `to`
column.
* The `state` table will be populated when a proxy is created or modified, with
a status of `DIRTY` and an updated last-modified timestamp. This table is used
by the `spindle-generate` module, but other tools could make use of it also.
* When entities are moved, the `index`, `triggers`, `audiences`, `membership`,
`media`, `index_media` and `about` tables are all updated if they have been
populated (although the `spindle-correlate` module will not populate them
itself).

## Rule-base

All of the Spindle modules for Twine use a rule-base to govern certain
aspects of their operation. The rule-base is installed to the Twine modules
directory, which is by default `${exec_prefix}/lib/twine`, and is named
`rulebase.ttl`.

The operation of the `spindle-correlate` module is controlled by the triples
in the rule-base which are in the form:

	ex:somepredicate spindle:coref spindle:matchingType .

For example:

	owl:sameAs spindle:coref spindle:resourceMatch .

These statements specify how exactly co-reference matches should be
determined. The simplest is `spindle:resourceMatch`, which states that the
predicate (the subject of this statement in the rule-base) expresses
a direct equivalence relationship between the subject and the object in a
statement it’s used in. The above example rule therefore states that when
statements are encountered in the form `<subject> owl:sameAs <object> .`, then
they should be interpreted as stating that `<subject>` and `<object>` are two
URIs for the same entity.

There is currently only one other matching type defined,
`spindle:wikipediaMatch`. This states that configured predicate is used to
express equivalence between a resource and _the subject of a particular Wikipedia page_.
When these statements are encountered, an attempt is made to translate them into
a direct equivalence relationship between the subject and the dbpedia.org
resource corresponding to that page. At present only English-language Wikipedia
links are supported.

For example, given a rule stating:—

	gn:wikipediaArticle spindle:coref spindle:wikipediaMatch .

If the following statement were processed:—

	<http://sws.geonames.org/2643743/> gn:wikipediaArticle <http://en.wikipedia.org/wiki/London> 


Then this would be interpeted as being equivalent to the following:

	<http://sws.geonames.org/2643743/> owl:sameAs <http://dbpedia.org/resource/London> .

## Configuration

You must specify a base graph URI for any identifiers generated by this module,
using the `graph` parameter in the `[spindle]` configuration section. When
information about a resource is processed, `spindle-correlate` will generate
a new URI, derived from this base URI, for each distinct resource that it
encounters.

By default, `spindle-correlate` will use the SPARQL connection details that
Twine has been configured with and use SPARQL Update to store information
about the co-references. The statements take the form (expressed below as
TriG, with `http://spindle.example.com/` as the configured base graph URI):


	<http://spindle.example.com/> {
	
		<http://dbpedia.org/resource/London> owl:sameAs <http://spindle.example.com/978649962c6f4a06a9b7a8c6b15548af#id> .
		
	}

If you want to store the co-references in a SQL database instead, you should
add a database connection URI to the `spindle` configuration section. Currently
only PostgreSQL databases are supported.

	[spindle]
	graph=http://spindle.example.com/
	db=pgsql://user:pass@10.50.25.3/spindle
