# Publishing the documentation

 - Install Sphinx; `sudo pip install sphinx`
 - Clone gh-pages branch: `cd doc; git clone -b gh-pages git@github.com:larsimmisch/pyalsaaudio.git gh-pages`
 - `cd doc; make publish`

# Release procedure

 - Update version number in setup.py
 - Create tag and push it, i.e. `git tag x.y.z; git push origin x.y.z`
 - `python setup.py sdist upload -r pypi` 
