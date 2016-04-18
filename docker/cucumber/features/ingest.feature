#encoding: utf-8
Feature: Ingesting nquad files into Twine/Spindle

Background:
	Given a running instance of "Quilt" at port 80
	Given a running instance of "Twine" at port 8000

Scenario Outline: Ingesting nquads
	When "<file>" is ingested into Twine
	Then "<proxies>" proxies should exist in the database

	Examples: Test files
		| file | proxies |
		| test.nq | 4 |
