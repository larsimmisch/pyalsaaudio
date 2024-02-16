# Make a new release

Update the version in setup.py

    pyalsa_version = '0.9.0'

Commit and push the update.

Create and push a tag naming the version (i.e. 0.9.0):

    git tag 0.9.0
    git push origin 0.9.0

Create the package:

    python3 setup.py sdist

Upload the package

    twine upload dist/*

Don't forget to update the documentation.

# Publish the documentation

The documentation is published through the `gh-pages` branch.

To publish the documentation, you need to clone the `gh-pages` branch of this repository into
`doc/gh-pages`. In `doc`, do:

    git clone -b gh-pages git@github.com:larsimmisch/pyalsaaudio.git gh-pages

(This is a bit of a hack)

Once that is set up, you can publish new documentation using:

    make publish

Be careful when new files are generated, however, you will have to add them
manually to git.
