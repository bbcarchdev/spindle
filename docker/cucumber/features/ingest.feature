#encoding: utf-8
Feature: Ingesting nquad files into Twine/Spindle

Scenario Outline: Ingesting shakespeare sample
	When "<file>" is ingested into Twine
	Then "<proxies>" proxies of type "<type>" should exist in the database

	Examples: Test files
		| file | proxies | type |
		| shakespeare-sample.nq | 1 | collections | 
#		| shakespeare-sample.nq | 1 | collections |
#		| shakespeare-sample.nq | 3 | people |
#		| shakespeare-sample.nq | 2 | works |
		